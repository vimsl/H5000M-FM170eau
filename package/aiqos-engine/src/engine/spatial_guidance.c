/**
 * AiQoS v4.4 - 3D 空间寻星与对准引擎
 * 基于 4 路天线 RSRP 计算方位角/俯仰角偏移建议
 * 支持馈线损耗自动补偿 (1/3/5/10m)
 */
#include "spatial_guidance.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float comp_max(const float comp[4]) {
    float v = comp[0];
    for (int i = 1; i < 4; i++) if (comp[i] > v) v = comp[i];
    return v;
}

static float comp_min(const float comp[4]) {
    float v = comp[0];
    for (int i = 1; i < 4; i++) if (comp[i] < v) v = comp[i];
    return v;
}

/* ============================================================
 * BUGFIX 2026-08-28 15:20 v4.5: 评分算法重构为 Sigmoid 平滑模型
 * 背景: 旧算法存在"断崖"——omni 分支 imbalance*5 线性惩罚过狠
 *       (极差 18.4dB 直接扣 92 分→21分), directional 分支无 Rank 参与;
 *       用户反馈 Rank1+单路差时得分从 76 崩到 28,误导寻星。
 * 新模型(对准度导向 + Sigmoid 平滑,消除一票否决):
 *   Score = 50·(1-ΔRSRP/30) + 20·f(Rank) + 20·σ(RSRP_best) + 10·σ(SINR)
 *   - 平衡度 50 分: 天线极差 30dB 归零, 极差≤6dB 接近满分
 *   - Rank 阶梯 20 分: Rank4=20 / Rank2=15 / Rank1=8 (绝不 4 折/扣 30)
 *   - RSRP 20 分: σ 拐点 -88dBm, -80~-95 平滑过渡
 *   - SINR 10 分: σ 拐点 10dB, 0~20 连续映射
 *   - 弱信号保护: SINR<5 时平衡度按比例衰减(对准度不可信)
 * 面板同向天线验证(2026-08-28): 4路极差天然1~3dB,平衡度恒高分,
 *   评分由 RSRP/SINR 主导——符合"旋转对准后信号变强则得分升"的物理。
 * ============================================================ */
static inline float aiqos_sigmoid(float x, float center, float k) {
    return 1.0f / (1.0f + expf(-k * (x - center)));
}

static int aiqos_alignment_score(const float comp[4], float sinr, int rank) {
    float rsrp_best = comp_max(comp);
    float imbalance = comp_max(comp) - comp_min(comp);

    float s_balance = (1.0f - (imbalance / 30.0f)) * 50.0f;
    if (s_balance < 0.0f) s_balance = 0.0f;

    float s_rank = (rank >= 4) ? 20.0f : ((rank >= 2) ? 15.0f : 8.0f);

    float s_sig  = aiqos_sigmoid(rsrp_best, -88.0f, 0.15f) * 20.0f;
    float s_sinr = aiqos_sigmoid(sinr, 10.0f, 0.25f) * 10.0f;

    if (sinr < 5.0f) s_balance *= (sinr / 5.0f);

    float total = s_balance + s_rank + s_sig + s_sinr;
    if (total > 100.0f) total = 100.0f;
    if (total < 10.0f)  total = 10.0f;
    return (int)(total + 0.5f);
}

SpatialGuidanceResult calculate_3d_alignment(const AntennaRawTelemetry *tele,
                                              bool is_directional) {
    SpatialGuidanceResult res = {0};

    /* 馈线损耗补偿 (3.5GHz N78: RG174 约 1.2dB/m) */
    float cable_loss = tele->feeder_length_m * 1.2f;
    float comp[4];
    for (int i = 0; i < 4; i++) comp[i] = tele->rsrp[i] + cable_loss;

    /* 水平方位: Ant0/1 (前) vs Ant2/3 (后) */
    float front = (comp[0] + comp[1]) / 2.0f;
    float rear  = (comp[2] + comp[3]) / 2.0f;
    float h_delta = front - rear;

    /* 垂直俯仰: Ant0/2 (顶) vs Ant1/3 (底) */
    float top    = (comp[0] + comp[2]) / 2.0f;
    float bottom = (comp[1] + comp[3]) / 2.0f;
    float v_delta = top - bottom;

    res.delta_v_h = fabsf(comp[0] - comp[1]);
    res.delta_h_v = fabsf(comp[2] - comp[3]);

    float spread = comp_max(comp) - comp_min(comp);
    res.rank = (tele->ri >= 1 && tele->ri <= 4)
               ? tele->ri
               : (spread < 3.5f ? 4 : spread < 6.0f ? 3 : spread < 10.0f ? 2 : 1);

    if (is_directional) {
        res.azimuth_offset_deg = -h_delta * 2.5f;
        res.elevation_offset_deg = v_delta * 3.0f;

        /* v4.5: 统一 Sigmoid 平滑评分(directional/omni 共用),消除断崖 */
        res.score = aiqos_alignment_score(comp, tele->sinr, res.rank);

        snprintf(res.advice, sizeof(res.advice),
                 "Directional: azimuth %.1f deg, elevation %.1f deg (cable +%.1fdB)",
                 res.azimuth_offset_deg, res.elevation_offset_deg, cable_loss);
    } else {
        float imbalance = comp_max(comp) - comp_min(comp);
        /* v4.5: 统一 Sigmoid 平滑评分,替代旧 imbalance*5 线性惩罚 */
        res.score = aiqos_alignment_score(comp, tele->sinr, res.rank);
        snprintf(res.advice, sizeof(res.advice),
                 "Omni: antenna imbalance %.1fdB, avoid metal obstructions", imbalance);
    }

    return res;
}

void write_spatial_files(const SpatialGuidanceResult *res,
                         const AntennaRawTelemetry *tele,
                         const char *failsafe_status) {
    FILE *f;

    f = fopen("/tmp/aiqos_spatial_score", "w");
    if (f) { fprintf(f, "%d", res->score); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_azimuth", "w");
    if (f) { fprintf(f, "%.1f", res->azimuth_offset_deg); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_elevation", "w");
    if (f) { fprintf(f, "%.1f", res->elevation_offset_deg); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_delta_v_h", "w");
    if (f) { fprintf(f, "%.1f", res->delta_v_h); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_delta_h_v", "w");
    if (f) { fprintf(f, "%.1f", res->delta_h_v); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_rank", "w");
    if (f) { fprintf(f, "%d", res->rank); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_failsafe", "w");
    if (f) { fprintf(f, "%s", failsafe_status ? failsafe_status : "auto"); fclose(f); }
    f = fopen("/tmp/aiqos_spatial_advice", "w");
    if (f) { fprintf(f, "%s", res->advice); fclose(f); }

    for (int i = 0; i < 4; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/tmp/aiqos_spatial_rsrp%d", i);
        f = fopen(path, "w");
        if (f) { fprintf(f, "%.1f", tele->rsrp[i]); fclose(f); }
    }
}
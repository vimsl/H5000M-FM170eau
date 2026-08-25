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

        float base = (comp[0] + 120.0f) * 1.5f + (tele->sinr / 30.0f) * 40.0f;
        res.score = (int)CLAMP(base - tele->pci_switch_rate * 5.0f, 0.0f, 100.0f);

        snprintf(res.advice, sizeof(res.advice),
                 "Directional: azimuth %.1f deg, elevation %.1f deg (cable +%.1fdB)",
                 res.azimuth_offset_deg, res.elevation_offset_deg, cable_loss);
    } else {
        float imbalance = comp_max(comp) - comp_min(comp);
        res.score = (int)CLAMP(100.0f - imbalance * 5.0f + (tele->sinr / 30.0f) * 30.0f, 0.0f, 100.0f);
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
/**
 * AiQoS v4.4 - PID controller (P1 fast-track compatible)
 *
 * Key P1 change: soft_cap decay is now dt-scaled to maintain
 * the same wall-clock time constant regardless of control frequency.
 */
#include "pid.h"
#include <math.h>

void pid_init(PIDController *pid, const ScenarioConfig *cfg) {
    pid->Kp = cfg->Kp;
    pid->Ki = cfg->Ki;
    pid->Kd = cfg->Kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_cqi = 0.0f;
    pid->soft_cap = cfg->soft_cap_init;
    pid->frozen = false;
    pid->freeze_ticks = 0;
    pid->I_max = 50000.0f;
}

float pid_update(PIDController *pid, float kalman_est, uint32_t measured_kbit,
                 int cqi, float dt, const ScenarioConfig *cfg,
                 uint32_t ambr_dl_kbps) { (void)ambr_dl_kbps;
    if (dt <= 0.0f) dt = 0.05f;

    float error = kalman_est - (float)measured_kbit;

    /* CQI derivative lookahead */
    float d_cqi = ((float)cqi - pid->prev_cqi) / dt;
    pid->prev_cqi = (float)cqi;

    /* Integral: handover freeze + Anti-Windup */
    if (cfg->freeze_on_handover && pid->frozen) {
        pid->freeze_ticks--;
        if (pid->freeze_ticks <= 0) pid->frozen = false;
        pid->integral = 0.0f;
    } else {
        pid->integral += cfg->Ki * error * dt;
        pid->integral = CLAMP(pid->integral, -pid->I_max, pid->I_max);
    }

    /* Derivative: error derivative + CQI lookahead */
    float d_error = (error - pid->prev_error) / dt;
    float derivative = cfg->Kd * d_error + cfg->cqi_drop_alpha * d_cqi;
    pid->prev_error = error;

    /* Output */
    float output = cfg->Kp * error + pid->integral + derivative;

    /*
     * P1 fix: Scale soft_cap decay to dt.
     * Original: decay=0.98 per 2s cycle -> tau ≈ 99 cycles = 198s
     * Now: decay^(dt/2.0) per cycle -> same wall-clock tau regardless of rate
     *
     * BUGFIX 2026-08-27: 原实现 soft_cap 只衰减不回升, 每周期乘以 decay(<1),
     * 从 soft_cap_init(500Mbps) 约 12 分钟单向衰减到接近 min_bw(14Mbps),
     * pid_get_target() 中 cap=min(max_bw, AMBR, soft_cap) 被 soft_cap 压死,
     * 造成 CAKE 限速恶性循环(空载 fast_rx=0 -> 无回升机制 -> 越用越慢)。
     * 修复: 以信道容量估计 kalman_est 为锚点双向校准 —— 容量高于 soft_cap 时回升,
     * 容量低于时才衰减; 并加下限保护, 防止衰减到荒谬低值。
     * 对应 v5.5 方案"三档基线锚点"(base_bw 自学习, 避免从极高上限向下缓慢收敛)。
     */
    float ref_dt = 2.0f;  /* original reference period */
    if (kalman_est > pid->soft_cap) {
        /* 信道容量高于当前 soft_cap: 回升(每 2s 约 +10%, 快速响应信号变好) */
        pid->soft_cap *= (1.0f + 0.10f * (dt / ref_dt));
        if (pid->soft_cap > cfg->soft_cap_init)
            pid->soft_cap = cfg->soft_cap_init;
    } else {
        /* 信道容量低于 soft_cap: 缓慢衰减 */
        float scaled_decay = powf(cfg->soft_cap_decay, dt / ref_dt);
        pid->soft_cap *= scaled_decay;
    }

    /* 下限保护: soft_cap 不低于 max(2*min_bw, 0.8*容量估计), 防止衰减到荒谬低值 */
    float cap_floor = (float)cfg->min_bw_kbit * 2.0f;
    if (kalman_est > 0 && kalman_est * 0.8f > cap_floor)
        cap_floor = kalman_est * 0.8f;
    if (pid->soft_cap < cap_floor)
        pid->soft_cap = cap_floor;
    if (pid->soft_cap > cfg->soft_cap_init)
        pid->soft_cap = cfg->soft_cap_init;

    return output;
}

void pid_freeze(PIDController *pid, int ticks) {
    pid->frozen = true;
    pid->freeze_ticks = ticks;
    pid->integral = 0.0f;
}

void pid_reset(PIDController *pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->frozen = false;
    pid->freeze_ticks = 0;
}

uint32_t pid_get_target(const PIDController *pid, const ScenarioConfig *cfg,
                        uint32_t ambr_dl_kbps) { (void)ambr_dl_kbps;
    /* B_cap = min(B_max, AMBR_DL, soft_cap), but never below min_bw */
    uint32_t cap = cfg->max_bw_kbit;
    if (ambr_dl_kbps > 0 && ambr_dl_kbps < cap) cap = ambr_dl_kbps;
    uint32_t soft = (uint32_t)pid->soft_cap;
    if (soft < cap) cap = soft;

    /* P1 fix: ensure cap never drops below min_bw_kbit */
    if (cap < cfg->min_bw_kbit) cap = cfg->min_bw_kbit;

    float raw = pid->Kp * pid->prev_error + pid->integral;
    uint32_t target = (uint32_t)CLAMP(raw, (float)cfg->min_bw_kbit, (float)cap);
    return target;
}
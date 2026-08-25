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
     */
    float ref_dt = 2.0f;  /* original reference period */
    float scaled_decay = powf(cfg->soft_cap_decay, dt / ref_dt);
    pid->soft_cap *= scaled_decay;

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
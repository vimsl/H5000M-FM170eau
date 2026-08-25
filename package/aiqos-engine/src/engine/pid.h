#ifndef AIQOS_PID_H
#define AIQOS_PID_H

#include "aiqos_types.h"

typedef struct {
    float Kp, Ki, Kd;
    float integral;
    float prev_error;
    float prev_cqi;
    float soft_cap;
    bool  frozen;
    int   freeze_ticks;
    float I_max;
} PIDController;

void pid_init(PIDController *pid, const ScenarioConfig *cfg);
float pid_update(PIDController *pid, float kalman_est, uint32_t measured_kbit,
                 int cqi, float dt, const ScenarioConfig *cfg,
                 uint32_t ambr_dl_kbps);
void pid_freeze(PIDController *pid, int ticks);
void pid_reset(PIDController *pid);
uint32_t pid_get_target(const PIDController *pid, const ScenarioConfig *cfg,
                        uint32_t ambr_dl_kbps);

#endif
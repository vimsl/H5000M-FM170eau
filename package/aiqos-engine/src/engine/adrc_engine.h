#ifndef ADRC_ENGINE_H
#define ADRC_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RESTRICT
#define RESTRICT __restrict__
#endif

typedef struct {
    float w0;
    float b0;
    float beta1, beta2, beta3;
    float alpha1, alpha2;
    float delta;
} adrc_params_t;

typedef struct {
    /* Tracking Differentiator states */
    float v1, v2;
    /* Extended State Observer states */
    float z1, z2, z3;
    /* Control output */
    float last_output;
} adrc_eso_t;

typedef struct {
    adrc_params_t params;
    adrc_eso_t eso;
    int calibrated;
} adrc_controller_t;

typedef struct {
    float u[256];
    float y[256];
    int   count;
} b0_calibration_samples_t;

void adrc_configure_params(adrc_controller_t *ctrl, float w0, float b0);
bool adrc_calibrate_b0(adrc_controller_t *ctrl,
                       const b0_calibration_samples_t *samples);
bool adrc_calib_run(adrc_controller_t *ctrl, long long base_rate_bps,
                    int max_secs, int interval_s);
float adrc_compute_control(adrc_controller_t *ctrl,
                           float target_capacity, float harq_penalty,
                           float dt);
void adrc_eso_update(adrc_controller_t *ctrl, float y, float u, float dt);
void adrc_reset(adrc_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif
/*
 * adrc_engine.c - ADRC engine (v60 shadow mode, Mbps-normalized)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <syslog.h>
#include "adrc_engine.h"

static inline float fast_fal(float e, float alpha, float delta) {
    float abs_e = fabsf(e);
    if (__builtin_expect(abs_e <= delta, 0))
        return e / powf(delta, 1.0f - alpha);
    return (e > 0.0f ? 1.0f : -1.0f) * powf(abs_e, alpha);
}

void adrc_configure_params(adrc_controller_t *ctrl, float w0, float b0) {
    if (w0 < 10.0f) w0 = 10.0f;
    if (w0 > 200.0f) w0 = 200.0f;
    ctrl->params.w0    = w0;
    ctrl->params.beta1 = w0;
    ctrl->params.beta2 = w0 * w0 / 2.2f;
    ctrl->params.beta3 = w0 * w0 * w0 / 17.6f;
    ctrl->params.alpha1 = 0.50f;
    ctrl->params.alpha2 = 0.25f;
    ctrl->params.delta  = 0.05f;
    ctrl->params.b0    = (fabsf(b0) < 1e-6f) ? 1.0f : b0;
}

void adrc_reset(adrc_controller_t *ctrl) {
    memset(&ctrl->eso, 0, sizeof(adrc_eso_t));
    ctrl->calibrated = 0;
}

bool adrc_calibrate_b0(adrc_controller_t *ctrl,
                       const b0_calibration_samples_t *samples) {
    if (ctrl == NULL || samples == NULL) return false;
    if (samples->count < 50) {
        syslog(LOG_WARNING, "ADRC: b0 calib failed, samples %d < 50", samples->count);
        return false;
    }
    double sum_uu = 0.0, sum_uy = 0.0;
    for (int i = 0; i < samples->count; i++) {
        float u = samples->u[i];
        float y = samples->y[i];
        if (!isfinite(u) || !isfinite(y)) continue;
        sum_uu += (double)u * (double)u;
        sum_uy += (double)u * (double)y;
    }
    if (sum_uu < 1e-12) {
        syslog(LOG_WARNING, "ADRC: b0 calib failed, insufficient excitation");
        return false;
    }
    float b0_hat = (float)(sum_uy / sum_uu);
    if (!isfinite(b0_hat) || b0_hat < 0.01f || b0_hat > 10.0f) {
        syslog(LOG_WARNING, "ADRC: b0=%.4f out of range [0.01,10.0]", b0_hat);
        return false;
    }
    ctrl->params.b0 = b0_hat;
    ctrl->calibrated = 1;
    syslog(LOG_INFO, "ADRC: b0 calibrated = %.4f (%d samples)", b0_hat, samples->count);
    return true;
}

void adrc_eso_update(adrc_controller_t *ctrl, float y_mbps, float u_mbps, float dt) {
    /* All states and signals are in Mbps */
    float *z1 = &ctrl->eso.z1;
    float *z2 = &ctrl->eso.z2;
    float *z3 = &ctrl->eso.z3;
    float e  = *z1 - y_mbps;
    float fe  = fast_fal(e, 0.50f, ctrl->params.delta);
    float fe1 = fast_fal(e, 0.25f, ctrl->params.delta);
    *z1 += dt * (*z2 - ctrl->params.beta1 * e);
    *z2 += dt * (*z3 - ctrl->params.beta2 * fe + ctrl->params.b0 * u_mbps);
    *z3 += dt * (-ctrl->params.beta3 * fe1);
    /* Stability clamps for dt=0.1s discrete ESO */
    if (*z1 < 0.0f) *z1 = 0.0f;
    if (*z1 > 1500.0f) *z1 = 1500.0f;
    if (*z2 > 5000.0f) *z2 = 5000.0f;
    if (*z2 < -5000.0f) *z2 = -5000.0f;
    if (*z3 > 2000.0f) *z3 = 2000.0f;
    if (*z3 < -2000.0f) *z3 = -2000.0f;
    /* NaN/inf guard */
    if (!isfinite(*z1) || !isfinite(*z2) || !isfinite(*z3)) {
        *z1 = y_mbps; *z2 = 0.0f; *z3 = 0.0f;
    }
}

float adrc_compute_control(adrc_controller_t *ctrl,
                           float target_bps, float harq_penalty,
                           float dt) {
    if (ctrl == NULL || !isfinite(dt) || dt <= 0.0f ||
        !isfinite(ctrl->params.b0) || fabsf(ctrl->params.b0) < 1e-6f)
        return 10000000.0f;

    /* Normalize to Mbps */
    float target_m = target_bps / 1000000.0f;
    if (!isfinite(target_m) || target_m < 1.0f) target_m = 10.0f;

    /* TD: smooth target transition (states in Mbps) */
    float fh = fast_fal(ctrl->eso.v1 - target_m, 0.5f, ctrl->params.delta);
    ctrl->eso.v1 += dt * ctrl->eso.v2;
    ctrl->eso.v2 += dt * (-10.0f * fh);
    if (!isfinite(ctrl->eso.v1)) ctrl->eso.v1 = target_m;
    if (!isfinite(ctrl->eso.v2)) ctrl->eso.v2 = 0.0f;
    /* Clamp TD states */
    if (ctrl->eso.v1 > 1200.0f) ctrl->eso.v1 = 1200.0f;
    if (ctrl->eso.v1 < 0.0f) ctrl->eso.v1 = 0.0f;
    if (ctrl->eso.v2 > 100000.0f) ctrl->eso.v2 = 100000.0f;
    if (ctrl->eso.v2 < -100000.0f) ctrl->eso.v2 = -100000.0f;

    /* State error */
    float e1 = ctrl->eso.v1 - ctrl->eso.z1;
    float e2 = ctrl->eso.v2 - ctrl->eso.z2;

    /* NLSEF */
    float u0 = 1.25f * fast_fal(e1, 0.75f, ctrl->params.delta) +
               0.45f * fast_fal(e2, 1.25f, ctrl->params.delta);
    if (!isfinite(u0)) u0 = 100.0f;
    if (u0 > 200.0f) u0 = 200.0f;
    if (u0 < -200.0f) u0 = -200.0f;

    /* Disturbance compensation (b0 dimension: bps rate -> bps output) */
    /* b0 was calibrated on raw bps, but we work in Mbps, so u_mbps = u0 / (b0 * 1e6) */
    /* Feedforward: start from target, ADRC adds disturbance correction only */
    float u_m = target_m + (u0 - ctrl->eso.z3) / 1.0f;
    u_m *= harq_penalty;

    if (!isfinite(u_m)) u_m = 10.0f;
    if (u_m < 10.0f)   u_m = 10.0f;
    if (u_m > 1200.0f) u_m = 1200.0f;

    ctrl->eso.last_output = u_m;
    return u_m * 1000000.0f; /* return in bps */
}
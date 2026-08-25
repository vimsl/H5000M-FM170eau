/**
 * AiQoS v4.3 - QCI 加权卡尔曼滤波器
 * 结合香农公式映射瞬时理论容量与 QCI/5QI 调度优先级因子
 */
#include "kalman.h"
#include <math.h>
#include <stdbool.h>

void kalman_init(KalmanFilter *kf, float q, float r) {
    kf->x = 0.0f;
    kf->p = 1.0f;
    kf->q = q;
    kf->r = r;
    kf->k = 0.0f;
    kf->init = false;
}

float kalman_update(KalmanFilter *kf, float z) {
    if (!kf->init) {
        kf->x = z;
        kf->init = true;
        return kf->x;
    }
    kf->p += kf->q;
    kf->k = kf->p / (kf->p + kf->r);
    kf->x += kf->k * (z - kf->x);
    kf->p *= (1.0f - kf->k);
    return kf->x;
}

void kalman_set_params(KalmanFilter *kf, float q, float r) {
    kf->q = q;
    kf->r = r;
}

/**
 * QCI/5QI 权重映射 (3GPP TS 23.501)
 *   5QI=1  VoIP:          1.05
 *   5QI=9  默认互联网:     1.00
 *   5QI=6/7/8 低优先级:    0.92
 */
/* qci_weight: called from aiqosd.c */


/**
 * 香农公式: z_k = w_qci * RI * W * log2(1 + 10^(SINR/10)) * eta_code
 * W = 100MHz (单载波带宽估计), eta_code = CQI/15 (编码效率)
 */
float kalman_estimate_capacity(const ShmTelemetry *tele, float qci_w) {
    float snr_linear = powf(10.0f, tele->sinr / 10.0f);
    float W = 100000.0f;  /* 100 MHz in kHz */
    float eta_code = (float)tele->cqi / 15.0f;
    if (eta_code < 0.1f) eta_code = 0.1f;

    return qci_w * (float)tele->ri * W * log2f(1.0f + snr_linear) * eta_code;
}
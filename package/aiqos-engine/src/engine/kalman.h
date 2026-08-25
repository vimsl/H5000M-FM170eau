#ifndef AIQOS_KALMAN_H
#define AIQOS_KALMAN_H

#include "aiqos_types.h"

typedef struct {
    float x;       /* 状态估计 (信道容量 kbps) */
    float p;       /* 估计误差协方差 */
    float q;       /* 过程噪声 (场景动态调整) */
    float r;       /* 测量噪声 (场景动态调整) */
    float k;       /* 卡尔曼增益 */
    bool  init;
} KalmanFilter;

void kalman_init(KalmanFilter *kf, float q, float r);
float kalman_update(KalmanFilter *kf, float z);
void kalman_set_params(KalmanFilter *kf, float q, float r);
float kalman_estimate_capacity(const ShmTelemetry *tele, float qci_weight);

#endif
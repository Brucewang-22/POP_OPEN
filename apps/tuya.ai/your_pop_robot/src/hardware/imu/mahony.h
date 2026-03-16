/**
 * @file mahony.h
 * @brief Mahony complementary filter for IMU attitude (roll/pitch/yaw).
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __MAHONY_H__
#define __MAHONY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float two_kp;
    float two_ki;
    float integral_fb_x;
    float integral_fb_y;
    float integral_fb_z;
    float q0;
    float q1;
    float q2;
    float q3;
} MAHONY_FILTER_T;

void mahony_init(MAHONY_FILTER_T *filter, float kp, float ki);
void mahony_reset(MAHONY_FILTER_T *filter);
void mahony_update_imu(MAHONY_FILTER_T *filter, float gx_rad_s, float gy_rad_s, float gz_rad_s,
                       float ax_g, float ay_g, float az_g, float dt_s);
void mahony_get_euler_deg(const MAHONY_FILTER_T *filter, float *roll_deg, float *pitch_deg,
                          float *yaw_deg);

#ifdef __cplusplus
}
#endif

#endif /* __MAHONY_H__ */

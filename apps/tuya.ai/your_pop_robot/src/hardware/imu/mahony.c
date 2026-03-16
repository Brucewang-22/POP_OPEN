/**
 * @file mahony.c
 * @brief Mahony filter implementation for IMU attitude.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "mahony.h"

#include <math.h>

#define MAHONY_PI 3.14159265358979323846f

static float __inv_sqrt(float x)
{
    if (x <= 0.0f) {
        return 0.0f;
    }
    return 1.0f / sqrtf(x);
}

void mahony_reset(MAHONY_FILTER_T *filter)
{
    if (filter == NULL) {
        return;
    }
    filter->integral_fb_x = 0.0f;
    filter->integral_fb_y = 0.0f;
    filter->integral_fb_z = 0.0f;
    filter->q0            = 1.0f;
    filter->q1            = 0.0f;
    filter->q2            = 0.0f;
    filter->q3            = 0.0f;
}

void mahony_init(MAHONY_FILTER_T *filter, float kp, float ki)
{
    if (filter == NULL) {
        return;
    }
    filter->two_kp = 2.0f * kp;
    filter->two_ki = 2.0f * ki;
    mahony_reset(filter);
}

void mahony_update_imu(MAHONY_FILTER_T *filter, float gx_rad_s, float gy_rad_s, float gz_rad_s,
                       float ax_g, float ay_g, float az_g, float dt_s)
{
    float recip_norm = 0.0f;
    float half_vx = 0.0f, half_vy = 0.0f, half_vz = 0.0f;
    float half_ex = 0.0f, half_ey = 0.0f, half_ez = 0.0f;
    float qa = 0.0f, qb = 0.0f, qc = 0.0f;

    if (filter == NULL || dt_s <= 0.0f) {
        return;
    }

    if (!((ax_g == 0.0f) && (ay_g == 0.0f) && (az_g == 0.0f))) {
        recip_norm = __inv_sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
        if (recip_norm > 0.0f) {
            ax_g *= recip_norm;
            ay_g *= recip_norm;
            az_g *= recip_norm;

            half_vx = filter->q1 * filter->q3 - filter->q0 * filter->q2;
            half_vy = filter->q0 * filter->q1 + filter->q2 * filter->q3;
            half_vz = filter->q0 * filter->q0 - 0.5f + filter->q3 * filter->q3;

            half_ex = (ay_g * half_vz - az_g * half_vy);
            half_ey = (az_g * half_vx - ax_g * half_vz);
            half_ez = (ax_g * half_vy - ay_g * half_vx);

            if (filter->two_ki > 0.0f) {
                filter->integral_fb_x += filter->two_ki * half_ex * dt_s;
                filter->integral_fb_y += filter->two_ki * half_ey * dt_s;
                filter->integral_fb_z += filter->two_ki * half_ez * dt_s;
                gx_rad_s += filter->integral_fb_x;
                gy_rad_s += filter->integral_fb_y;
                gz_rad_s += filter->integral_fb_z;
            } else {
                filter->integral_fb_x = 0.0f;
                filter->integral_fb_y = 0.0f;
                filter->integral_fb_z = 0.0f;
            }

            gx_rad_s += filter->two_kp * half_ex;
            gy_rad_s += filter->two_kp * half_ey;
            gz_rad_s += filter->two_kp * half_ez;
        }
    }

    gx_rad_s *= 0.5f * dt_s;
    gy_rad_s *= 0.5f * dt_s;
    gz_rad_s *= 0.5f * dt_s;

    qa = filter->q0;
    qb = filter->q1;
    qc = filter->q2;

    filter->q0 += (-qb * gx_rad_s - qc * gy_rad_s - filter->q3 * gz_rad_s);
    filter->q1 += (qa * gx_rad_s + qc * gz_rad_s - filter->q3 * gy_rad_s);
    filter->q2 += (qa * gy_rad_s - qb * gz_rad_s + filter->q3 * gx_rad_s);
    filter->q3 += (qa * gz_rad_s + qb * gy_rad_s - qc * gx_rad_s);

    recip_norm = __inv_sqrt(filter->q0 * filter->q0 + filter->q1 * filter->q1 +
                            filter->q2 * filter->q2 + filter->q3 * filter->q3);
    if (recip_norm <= 0.0f) {
        mahony_reset(filter);
        return;
    }
    filter->q0 *= recip_norm;
    filter->q1 *= recip_norm;
    filter->q2 *= recip_norm;
    filter->q3 *= recip_norm;
}

void mahony_get_euler_deg(const MAHONY_FILTER_T *filter, float *roll_deg, float *pitch_deg,
                          float *yaw_deg)
{
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    float sinp = 0.0f;

    if (filter == NULL) {
        return;
    }

    roll = atan2f(2.0f * (filter->q0 * filter->q1 + filter->q2 * filter->q3),
                  1.0f - 2.0f * (filter->q1 * filter->q1 + filter->q2 * filter->q2));

    sinp = 2.0f * (filter->q0 * filter->q2 - filter->q3 * filter->q1);
    if (sinp >= 1.0f) {
        pitch = MAHONY_PI / 2.0f;
    } else if (sinp <= -1.0f) {
        pitch = -MAHONY_PI / 2.0f;
    } else {
        pitch = asinf(sinp);
    }

    yaw = atan2f(2.0f * (filter->q0 * filter->q3 + filter->q1 * filter->q2),
                 1.0f - 2.0f * (filter->q2 * filter->q2 + filter->q3 * filter->q3));

    if (roll_deg != NULL) {
        *roll_deg = roll * (180.0f / MAHONY_PI);
    }
    if (pitch_deg != NULL) {
        *pitch_deg = pitch * (180.0f / MAHONY_PI);
    }
    if (yaw_deg != NULL) {
        *yaw_deg = yaw * (180.0f / MAHONY_PI);
    }
}

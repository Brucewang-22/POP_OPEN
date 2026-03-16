/**
 * @file imu_ahrs.h
 * @brief IMU AHRS (attitude heading reference system) interface for your_pop_robot.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __IMU_AHRS_H__
#define __IMU_AHRS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    roll_deg;
    float    pitch_deg;
    float    yaw_deg;
    float    temp_c;
    uint32_t sample_count;
    uint8_t  who_am_i;
    bool     calibrated;
    float    ax_g;
    float    ay_g;
    float    az_g;
    float    gx_dps;
    float    gy_dps;
    float    gz_dps;
    float    ax_mps2;
    float    ay_mps2;
    float    az_mps2;
    float    accel_norm_mps2;
} IMU_AHRS_STATUS_T;

OPERATE_RET imu_ahrs_init(void);
OPERATE_RET imu_ahrs_poll(void);
OPERATE_RET imu_ahrs_get_status(IMU_AHRS_STATUS_T *status);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_AHRS_H__ */

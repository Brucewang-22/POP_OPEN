/**
 * @file imu_ahrs.c
 * @brief IMU AHRS: MPU6050 + Mahony filter for roll/pitch/yaw.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "imu_ahrs.h"

#include <math.h>
#include <string.h>

#include "mahony.h"
#include "mpu6050.h"
#include "tal_log.h"
#include "tal_system.h"

#define IMU_AHRS_DEG_TO_RAD     0.01745329251994329577f
#define IMU_AHRS_GRAVITY_MPS2   9.80665f
#define IMU_AHRS_DEFAULT_DT_S   0.02f
#define IMU_AHRS_MAX_DT_S       0.2f
#define IMU_AHRS_GYRO_CAL_COUNT 100

typedef struct {
    bool              inited;
    uint32_t          last_update_ms;
    float             gyro_bias_x_dps;
    float             gyro_bias_y_dps;
    float             gyro_bias_z_dps;
    MAHONY_FILTER_T   filter;
    IMU_AHRS_STATUS_T status;
} IMU_AHRS_CTX_T;

static IMU_AHRS_CTX_T sg_imu_ctx;

static float __sample_temp_c(const MPU6050_SAMPLE_T *sample)
{
    return mpu6050_temp_c_from_raw(sample->temp_raw);
}

static OPERATE_RET __gyro_calibrate(void)
{
    MPU6050_SAMPLE_T sample = {0};
    float bias_x = 0.0f, bias_y = 0.0f, bias_z = 0.0f;

    PR_NOTICE("imu_ahrs gyro calibration: keep sensor still");
    for (uint32_t i = 0; i < IMU_AHRS_GYRO_CAL_COUNT; i++) {
        OPERATE_RET rt = mpu6050_read_sample(&sample);
        if (rt != OPRT_OK) {
            PR_ERR("imu_ahrs calibration sample failed: %d", rt);
            return rt;
        }
        bias_x += (float)sample.gyro_x / MPU6050_GYRO_LSB_PER_DPS;
        bias_y += (float)sample.gyro_y / MPU6050_GYRO_LSB_PER_DPS;
        bias_z += (float)sample.gyro_z / MPU6050_GYRO_LSB_PER_DPS;
        tal_system_sleep(5);
    }

    sg_imu_ctx.gyro_bias_x_dps = bias_x / (float)IMU_AHRS_GYRO_CAL_COUNT;
    sg_imu_ctx.gyro_bias_y_dps = bias_y / (float)IMU_AHRS_GYRO_CAL_COUNT;
    sg_imu_ctx.gyro_bias_z_dps = bias_z / (float)IMU_AHRS_GYRO_CAL_COUNT;
    sg_imu_ctx.status.calibrated = true;

    PR_NOTICE("imu_ahrs gyro calibration done: bias_dps=%.3f/%.3f/%.3f",
              sg_imu_ctx.gyro_bias_x_dps, sg_imu_ctx.gyro_bias_y_dps, sg_imu_ctx.gyro_bias_z_dps);
    return OPRT_OK;
}

OPERATE_RET imu_ahrs_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_imu_ctx.inited) {
        return OPRT_OK;
    }

    memset(&sg_imu_ctx, 0, sizeof(sg_imu_ctx));

    rt = mpu6050_init();
    if (rt != OPRT_OK) {
        PR_ERR("imu_ahrs_init: mpu6050_init failed: %d", rt);
        return rt;
    }

    rt = mpu6050_read_who_am_i(&sg_imu_ctx.status.who_am_i);
    if (rt != OPRT_OK) {
        PR_ERR("imu_ahrs_init: read who_am_i failed: %d", rt);
        return rt;
    }
    PR_NOTICE("imu_ahrs detected who=0x%02X", sg_imu_ctx.status.who_am_i);

    mahony_init(&sg_imu_ctx.filter, 1.0f, 0.0f);

    rt = __gyro_calibrate();
    if (rt != OPRT_OK) {
        return rt;
    }

    sg_imu_ctx.last_update_ms = (uint32_t)tal_system_get_millisecond();
    sg_imu_ctx.inited         = true;
    return OPRT_OK;
}

OPERATE_RET imu_ahrs_poll(void)
{
    MPU6050_SAMPLE_T sample = {0};
    uint32_t now_ms;
    float dt_s = IMU_AHRS_DEFAULT_DT_S;
    float ax_g = 0.0f, ay_g = 0.0f, az_g = 0.0f;
    float gx_dps = 0.0f, gy_dps = 0.0f, gz_dps = 0.0f;
    OPERATE_RET rt;

    if (!sg_imu_ctx.inited) {
        return OPRT_COM_ERROR;
    }

    now_ms = (uint32_t)tal_system_get_millisecond();
    if (sg_imu_ctx.last_update_ms != 0 && now_ms > sg_imu_ctx.last_update_ms) {
        dt_s = (float)(now_ms - sg_imu_ctx.last_update_ms) / 1000.0f;
        if (dt_s <= 0.0f || dt_s > IMU_AHRS_MAX_DT_S) {
            dt_s = IMU_AHRS_DEFAULT_DT_S;
        }
    }
    sg_imu_ctx.last_update_ms = now_ms;

    rt = mpu6050_read_sample(&sample);
    if (rt != OPRT_OK) {
        return rt;
    }

    ax_g = (float)sample.accel_x / MPU6050_ACCEL_LSB_PER_G;
    ay_g = (float)sample.accel_y / MPU6050_ACCEL_LSB_PER_G;
    az_g = (float)sample.accel_z / MPU6050_ACCEL_LSB_PER_G;

    gx_dps = ((float)sample.gyro_x / MPU6050_GYRO_LSB_PER_DPS) - sg_imu_ctx.gyro_bias_x_dps;
    gy_dps = ((float)sample.gyro_y / MPU6050_GYRO_LSB_PER_DPS) - sg_imu_ctx.gyro_bias_y_dps;
    gz_dps = ((float)sample.gyro_z / MPU6050_GYRO_LSB_PER_DPS) - sg_imu_ctx.gyro_bias_z_dps;

    mahony_update_imu(&sg_imu_ctx.filter,
                      gx_dps * IMU_AHRS_DEG_TO_RAD, gy_dps * IMU_AHRS_DEG_TO_RAD,
                      gz_dps * IMU_AHRS_DEG_TO_RAD, ax_g, ay_g, az_g, dt_s);

    mahony_get_euler_deg(&sg_imu_ctx.filter,
                         &sg_imu_ctx.status.roll_deg, &sg_imu_ctx.status.pitch_deg,
                         &sg_imu_ctx.status.yaw_deg);

    sg_imu_ctx.status.temp_c       = __sample_temp_c(&sample);
    sg_imu_ctx.status.sample_count++;
    sg_imu_ctx.status.ax_g          = ax_g;
    sg_imu_ctx.status.ay_g          = ay_g;
    sg_imu_ctx.status.az_g          = az_g;
    sg_imu_ctx.status.gx_dps        = gx_dps;
    sg_imu_ctx.status.gy_dps        = gy_dps;
    sg_imu_ctx.status.gz_dps        = gz_dps;
    sg_imu_ctx.status.ax_mps2      = ax_g * IMU_AHRS_GRAVITY_MPS2;
    sg_imu_ctx.status.ay_mps2      = ay_g * IMU_AHRS_GRAVITY_MPS2;
    sg_imu_ctx.status.az_mps2      = az_g * IMU_AHRS_GRAVITY_MPS2;
    sg_imu_ctx.status.accel_norm_mps2 = sqrtf(sg_imu_ctx.status.ax_mps2 * sg_imu_ctx.status.ax_mps2 +
                                              sg_imu_ctx.status.ay_mps2 * sg_imu_ctx.status.ay_mps2 +
                                              sg_imu_ctx.status.az_mps2 * sg_imu_ctx.status.az_mps2);

    return OPRT_OK;
}

OPERATE_RET imu_ahrs_get_status(IMU_AHRS_STATUS_T *status)
{
    if (status == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!sg_imu_ctx.inited) {
        return OPRT_COM_ERROR;
    }
    *status = sg_imu_ctx.status;
    return OPRT_OK;
}

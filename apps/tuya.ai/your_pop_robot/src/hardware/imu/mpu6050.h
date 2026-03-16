/**
 * @file mpu6050.h
 * @brief MPU6050 (and MPU6500/MPU9250 compatible) I2C driver for your_pop_robot IMU.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __MPU6050_H__
#define __MPU6050_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_I2C_ADDR_PRIMARY 0x68
#define MPU6050_I2C_ADDR_ALT     0x69

#define MPU6XXX_WHO_AM_I_6050     0x68
#define MPU6XXX_WHO_AM_I_6050_ALT 0x69
#define MPU6XXX_WHO_AM_I_6500     0x70

#define MPU6050_ACCEL_LSB_PER_G    16384.0f
#define MPU6050_GYRO_LSB_PER_DPS   131.0f
#define MPU6050_TEMP_SENSITIVITY   340.0f
#define MPU6050_TEMP_OFFSET_C      36.53f

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp_raw;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} MPU6050_SAMPLE_T;

OPERATE_RET mpu6050_init(void);
OPERATE_RET mpu6050_deinit(void);
OPERATE_RET mpu6050_read_who_am_i(uint8_t *who_am_i);
OPERATE_RET mpu6050_read_sample(MPU6050_SAMPLE_T *sample);
bool        mpu6050_is_inited(void);
uint8_t     mpu6050_get_cached_who_am_i(void);
float       mpu6050_temp_c_from_raw(int16_t temp_raw);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */

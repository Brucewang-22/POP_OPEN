/**
 * @file mpu6050.c
 * @brief MPU6050 I2C driver implementation. Compatible with WHO_AM_I 0x68/0x69/0x70.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "mpu6050.h"

#include <string.h>

#include "tal_log.h"
#include "tal_system.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#define MPU6050_I2C_PORT TUYA_I2C_NUM_0

#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_PWR_MGMT_2   0x6C
#define MPU6050_REG_WHO_AM_I     0x75

static bool    sg_inited;
static uint8_t sg_who_am_i;

static bool __mpu6050_is_supported_who_am_i(uint8_t who_am_i)
{
    return (who_am_i == MPU6XXX_WHO_AM_I_6050 || who_am_i == MPU6XXX_WHO_AM_I_6050_ALT ||
            who_am_i == MPU6XXX_WHO_AM_I_6500);
}

static const char *__mpu6050_device_name(uint8_t who_am_i)
{
    if (who_am_i == MPU6XXX_WHO_AM_I_6500) {
        return "MPU6500/MPU9250-compatible";
    }
    return "MPU6050-compatible";
}

static OPERATE_RET __mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return tkl_i2c_master_send(MPU6050_I2C_PORT, MPU6050_I2C_ADDR_PRIMARY, buf, sizeof(buf), FALSE);
}

static OPERATE_RET __mpu6050_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    OPERATE_RET rt;

    if (data == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_i2c_master_send(MPU6050_I2C_PORT, MPU6050_I2C_ADDR_PRIMARY, &reg, 1, TRUE);
    if (rt != OPRT_OK) {
        return rt;
    }
    return tkl_i2c_master_receive(MPU6050_I2C_PORT, MPU6050_I2C_ADDR_PRIMARY, data, len, FALSE);
}

bool mpu6050_is_inited(void)
{
    return sg_inited;
}

uint8_t mpu6050_get_cached_who_am_i(void)
{
    return sg_who_am_i;
}

float mpu6050_temp_c_from_raw(int16_t temp_raw)
{
    if (sg_who_am_i == MPU6XXX_WHO_AM_I_6500) {
        return ((float)temp_raw / 333.87f) + 21.0f;
    }
    return ((float)temp_raw / MPU6050_TEMP_SENSITIVITY) + MPU6050_TEMP_OFFSET_C;
}

OPERATE_RET mpu6050_read_who_am_i(uint8_t *who_am_i)
{
    if (who_am_i == NULL) {
        return OPRT_INVALID_PARM;
    }
    return __mpu6050_read_regs(MPU6050_REG_WHO_AM_I, who_am_i, 1);
}

OPERATE_RET mpu6050_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role      = TUYA_IIC_MODE_MASTER,
        .speed     = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    uint8_t who_am_i = 0;

    if (sg_inited) {
        return OPRT_OK;
    }

    rt = tkl_io_pinmux_config(TUYA_GPIO_NUM_20, TUYA_IIC0_SCL);
    if (rt != OPRT_OK) {
        PR_ERR("mpu6050 pinmux SCL failed: %d", rt);
        return rt;
    }
    rt = tkl_io_pinmux_config(TUYA_GPIO_NUM_21, TUYA_IIC0_SDA);
    if (rt != OPRT_OK) {
        PR_ERR("mpu6050 pinmux SDA failed: %d", rt);
        return rt;
    }
    rt = tkl_i2c_init(MPU6050_I2C_PORT, &i2c_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("mpu6050 i2c init failed: %d", rt);
        return rt;
    }

    rt = __mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80);
    if (rt != OPRT_OK) {
        PR_ERR("mpu6050 reset failed: %d", rt);
        return rt;
    }
    tal_system_sleep(100);

    rt = __mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __mpu6050_write_reg(MPU6050_REG_PWR_MGMT_2, 0x00);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __mpu6050_write_reg(MPU6050_REG_CONFIG, 0x03);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 0x04);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00);
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_system_sleep(20);

    rt = mpu6050_read_who_am_i(&who_am_i);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (!__mpu6050_is_supported_who_am_i(who_am_i)) {
        PR_ERR("mpu6050 unsupported who_am_i: 0x%02X", who_am_i);
        return OPRT_COM_ERROR;
    }

    sg_who_am_i = who_am_i;
    sg_inited   = true;
    PR_NOTICE("mpu6050_init ok: port=%d addr=0x%02X who=0x%02X dev=%s", (int)MPU6050_I2C_PORT,
              MPU6050_I2C_ADDR_PRIMARY, who_am_i, __mpu6050_device_name(who_am_i));
    return OPRT_OK;
}

OPERATE_RET mpu6050_deinit(void)
{
    if (!sg_inited) {
        return OPRT_OK;
    }
    sg_inited   = false;
    sg_who_am_i = 0;
    return tkl_i2c_deinit(MPU6050_I2C_PORT);
}

OPERATE_RET mpu6050_read_sample(MPU6050_SAMPLE_T *sample)
{
    uint8_t data[14] = {0};
    OPERATE_RET rt;

    if (sample == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!sg_inited) {
        return OPRT_COM_ERROR;
    }

    rt = __mpu6050_read_regs(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));
    if (rt != OPRT_OK) {
        return rt;
    }

    memset(sample, 0, sizeof(*sample));
    sample->accel_x  = (int16_t)((data[0] << 8) | data[1]);
    sample->accel_y  = (int16_t)((data[2] << 8) | data[3]);
    sample->accel_z  = (int16_t)((data[4] << 8) | data[5]);
    sample->temp_raw = (int16_t)((data[6] << 8) | data[7]);
    sample->gyro_x   = (int16_t)((data[8] << 8) | data[9]);
    sample->gyro_y   = (int16_t)((data[10] << 8) | data[11]);
    sample->gyro_z   = (int16_t)((data[12] << 8) | data[13]);

    return OPRT_OK;
}

/**
 * @file touch_sensor.h
 * @brief External touch switch sensor (VCC/GND/GPIO) hardware abstraction.
 *
 * Notes:
 * - This module is for an external touch chip that outputs a digital level on GPIO.
 * - It is NOT the T5 internal "touch test" pin.
 */
#pragma once

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOUCH_SENSOR_CH_MAX 3

typedef struct {
    TUYA_GPIO_NUM_E   gpio_pin;      /* Digital output pin from touch module */
    TUYA_GPIO_LEVEL_E active_level;  /* Level meaning "touched" */
    TUYA_GPIO_MODE_E  pull;          /* Input pull configuration */
} TOUCH_SENSOR_CFG_T;

OPERATE_RET touch_sensor_init_channel(uint8_t ch, const TOUCH_SENSOR_CFG_T *cfg);
OPERATE_RET touch_sensor_deinit(void);
OPERATE_RET touch_sensor_read_channel(uint8_t ch, bool *touched);
TUYA_GPIO_NUM_E touch_sensor_get_pin_channel(uint8_t ch);

/* Legacy single-channel API: channel 0 only. */
OPERATE_RET touch_sensor_init(const TOUCH_SENSOR_CFG_T *cfg);
OPERATE_RET touch_sensor_read(bool *touched);
TUYA_GPIO_NUM_E touch_sensor_get_pin(void);

#ifdef __cplusplus
}
#endif


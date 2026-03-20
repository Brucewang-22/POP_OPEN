/**
 * @file hardware_abstraction.h
 * @brief Hardware abstraction for interactive layer.
 *
 * Route hardware modules into two domains:
 * - input: microphone, touch, imu
 * - output: lcd, moto, audio
 */
#pragma once

#include "tuya_cloud_types.h"
#include "imu_ahrs.h"
#include "lcd_gc9d01.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_GPIO_NUM_E gpio_pin;
    TUYA_GPIO_LEVEL_E active_level;
    TUYA_GPIO_MODE_E pull;
} INTERACTIVE_TOUCH_CHANNEL_CFG_T;

typedef struct {
    OPERATE_RET (*microphone_init)(void);
    OPERATE_RET (*microphone_get_status)(uint32_t *frames_1s, uint32_t *rms_permille);
    OPERATE_RET (*touch_init_channel)(uint8_t ch, const INTERACTIVE_TOUCH_CHANNEL_CFG_T *cfg);
    OPERATE_RET (*touch_read_channel)(uint8_t ch, bool *touched);
    TUYA_GPIO_NUM_E (*touch_get_pin_channel)(uint8_t ch);
    OPERATE_RET (*imu_init)(void);
    OPERATE_RET (*imu_poll)(void);
    OPERATE_RET (*imu_get_status)(IMU_AHRS_STATUS_T *status);
} INTERACTIVE_INPUT_IF_T;

typedef struct {
    OPERATE_RET (*lcd_clear)(LCD_INSTANCE_E instance);
    OPERATE_RET (*lcd_fill_color)(LCD_INSTANCE_E instance, uint16_t color_rgb565);
    OPERATE_RET (*moto_init)(void);
    OPERATE_RET (*audio_start_test)(void);
} INTERACTIVE_OUTPUT_IF_T;

typedef struct {
    INTERACTIVE_INPUT_IF_T input;
    INTERACTIVE_OUTPUT_IF_T output;
} INTERACTIVE_HARDWARE_ABSTRACTION_T;

/**
 * @brief Get global hardware abstraction table.
 */
const INTERACTIVE_HARDWARE_ABSTRACTION_T *interactive_get_hardware_abstraction(void);

#ifdef __cplusplus
}
#endif

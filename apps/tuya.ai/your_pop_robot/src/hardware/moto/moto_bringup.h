/**
 * @file moto_bringup.h
 * @brief Servo bring-up: zero-based boot-home for POP robot motors.
 */
#pragma once

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init motor bring-up and run boot-home once.
 */
OPERATE_RET moto_bringup_init(void);

/**
 * @brief Set boot-home speed (degree/s), effective for next init/home run.
 */
OPERATE_RET moto_bringup_set_home_speed(uint16_t speed_dps);

/**
 * @brief Set home angle of one servo channel (index: 0->PWM2, 1->PWM3).
 */
OPERATE_RET moto_bringup_set_home_angle(uint8_t ch_index, int16_t angle_deg);

/**
 * @brief Get home angle of one servo channel (index: 0->PWM2, 1->PWM3).
 */
OPERATE_RET moto_bringup_get_home_angle(uint8_t ch_index, int16_t *angle_deg);

/**
 * @brief Get symmetric mechanical limit around home angle of one channel.
 */
OPERATE_RET moto_bringup_get_limit_deg(uint8_t ch_index, int16_t *limit_deg);

/**
 * @brief Resolve one channel logical offset around home into servo target angle.
 */
OPERATE_RET moto_bringup_resolve_target_angle(uint8_t ch_index, int16_t logical_offset_deg,
                                              int16_t *angle_deg);

#ifdef __cplusplus
}
#endif

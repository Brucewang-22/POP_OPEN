/**
 * @file moto_bringup.h
 * @brief Servo bring-up: smooth boot-home for POP robot motors.
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

#ifdef __cplusplus
}
#endif
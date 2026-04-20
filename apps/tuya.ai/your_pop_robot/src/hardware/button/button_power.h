/**
 * @file button_power.h
 * @brief Long-press power switch logic using P36 output and P37 button input.
 */
#pragma once

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool power_enabled;
    bool button_pressed;
    bool long_press_armed;
    uint32_t press_duration_ms;
} BUTTON_POWER_STATUS_T;

OPERATE_RET button_power_init(void);
OPERATE_RET button_power_poll(uint32_t elapsed_ms);
OPERATE_RET button_power_get_status(BUTTON_POWER_STATUS_T *status);

#ifdef __cplusplus
}
#endif

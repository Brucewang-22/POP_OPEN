/**
 * @file mic_bringup.h
 * @brief Microphone bring-up status interface for your_pop_robot.
 */
#pragma once

#include "tuya_cloud_types.h"
#include "ai_user_event.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET mic_bringup_init(void);
void mic_bringup_on_ai_event(AI_NOTIFY_EVENT_T *event);
OPERATE_RET mic_bringup_get_status(uint32_t *frames_1s, uint32_t *rms_permille);

#ifdef __cplusplus
}
#endif


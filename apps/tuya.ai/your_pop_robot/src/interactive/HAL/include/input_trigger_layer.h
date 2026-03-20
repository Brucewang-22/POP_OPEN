/**
 * @file input_trigger_layer.h
 * @brief Threshold trigger layer for interactive input hardware.
 */
#pragma once

#include "hardware_abstraction.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_TRIGGER_TOUCH_CH_MAX 3

typedef struct {
    uint32_t microphone_min_frames_1s;
    uint32_t microphone_min_rms_permille;
    uint8_t touch_min_active_channels;
    uint32_t touch_event_window_ms;
    uint8_t touch_min_events_in_window;
    bool imu_require_calibrated;
    float imu_accel_norm_min_mps2;
    float imu_accel_norm_max_mps2;
    float imu_motion_accel_delta_min_mps2;
    float imu_motion_gyro_axis_min_dps;
    uint8_t imu_motion_min_consecutive_hits;
} INPUT_TRIGGER_THRESHOLDS_T;

typedef struct {
    uint8_t microphone; /* 1: reached threshold, 0: not reached */
    uint8_t touch;      /* 1: reached threshold, 0: not reached */
    uint8_t imu;        /* 1: reached threshold, 0: not reached */
} INPUT_TRIGGER_FLAGS_T;

OPERATE_RET input_trigger_layer_init(const INTERACTIVE_HARDWARE_ABSTRACTION_T *hardware_abstraction);
OPERATE_RET input_trigger_layer_set_thresholds(const INPUT_TRIGGER_THRESHOLDS_T *thresholds);
OPERATE_RET input_trigger_layer_eval(void);
OPERATE_RET input_trigger_layer_get_flags(INPUT_TRIGGER_FLAGS_T *flags);

#ifdef __cplusplus
}
#endif

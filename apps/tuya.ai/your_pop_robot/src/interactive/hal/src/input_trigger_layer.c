/**
 * @file input_trigger_layer.c
 * @brief Threshold trigger layer for interactive input hardware.
 */

#include "input_trigger_layer.h"

#include <string.h>
#include "tal_api.h"
#include "hardware_thresholds_config.h"

#define INPUT_TRIGGER_GRAVITY_MPS2                             9.80665f

static const INTERACTIVE_HARDWARE_ABSTRACTION_T *sg_hw = NULL;
static INPUT_TRIGGER_THRESHOLDS_T sg_thresholds;
static INPUT_TRIGGER_FLAGS_T sg_flags;
static INPUT_TRIGGER_TOUCH_DETAIL_T sg_touch_detail;
static uint8_t sg_imu_motion_hits = 0;
static uint8_t sg_touch_prev_active = 0;
static uint8_t sg_touch_event_count = 0;
static uint64_t sg_touch_window_start_ms = 0;
static uint8_t sg_touch_prev_mask = 0;
static uint8_t sg_touch_prev_threshold = 0;

static float __float_abs(float v)
{
    return (v >= 0.0f) ? v : -v;
}

static void __set_default_thresholds(INPUT_TRIGGER_THRESHOLDS_T *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->microphone_min_frames_1s = HWCFG_MICROPHONE_MIN_FRAMES_1S;
    cfg->microphone_min_rms_permille = HWCFG_MICROPHONE_MIN_RMS_PERMILLE;
    cfg->touch_min_active_channels = HWCFG_TOUCH_MIN_ACTIVE_CHANNELS;
    cfg->touch_event_window_ms = HWCFG_TOUCH_EVENT_WINDOW_MS;
    cfg->touch_min_events_in_window = HWCFG_TOUCH_MIN_EVENTS_IN_WINDOW;
    cfg->imu_require_calibrated = HWCFG_IMU_REQUIRE_CALIBRATED;
    cfg->imu_accel_norm_min_mps2 = HWCFG_IMU_ACCEL_NORM_MIN_MPS2;
    cfg->imu_accel_norm_max_mps2 = HWCFG_IMU_ACCEL_NORM_MAX_MPS2;
    cfg->imu_motion_accel_delta_min_mps2 = HWCFG_IMU_MOTION_ACCEL_DELTA_MIN_MPS2;
    cfg->imu_motion_gyro_axis_min_dps = HWCFG_IMU_MOTION_GYRO_AXIS_MIN_DPS;
    cfg->imu_motion_min_consecutive_hits = HWCFG_IMU_MOTION_MIN_CONSECUTIVE_HITS;
}

static uint8_t __eval_microphone(void)
{
    uint32_t frames_1s = 0;
    uint32_t rms_permille = 0;

    if (sg_hw == NULL) {
        return 0;
    }
    if (sg_hw->input.microphone_get_status == NULL) {
        return 0;
    }
    if (sg_hw->input.microphone_get_status(&frames_1s, &rms_permille) != OPRT_OK) {
        return 0;
    }

    if (frames_1s >= sg_thresholds.microphone_min_frames_1s &&
        rms_permille >= sg_thresholds.microphone_min_rms_permille) {
        return 1;
    }
    return 0;
}

static uint8_t __eval_touch(void)
{
    uint8_t active_cnt = 0;
    bool touched = false;
    bool touch_active = false;
    uint8_t touch_active_mask = 0;
    uint8_t touch_rising_mask = 0;
    uint64_t now_ms = 0;
    uint64_t elapsed_ms = 0;
    uint8_t touch_threshold = 0;

    if (sg_hw == NULL) {
        memset(&sg_touch_detail, 0, sizeof(sg_touch_detail));
        return 0;
    }
    if (sg_hw->input.touch_read_channel == NULL) {
        memset(&sg_touch_detail, 0, sizeof(sg_touch_detail));
        return 0;
    }

    for (uint8_t ch = 0; ch < INPUT_TRIGGER_TOUCH_CH_MAX; ch++) {
        if (sg_hw->input.touch_read_channel(ch, &touched) != OPRT_OK) {
            continue;
        }
        if (touched) {
            active_cnt++;
            touch_active_mask |= (uint8_t)(1U << ch);
        }
    }
    touch_rising_mask = (uint8_t)(touch_active_mask & (uint8_t)(~sg_touch_prev_mask));
    sg_touch_prev_mask = touch_active_mask;

    touch_active = (active_cnt >= sg_thresholds.touch_min_active_channels);
    now_ms = tal_system_get_millisecond();

    if (sg_touch_event_count > 0) {
        elapsed_ms = now_ms - sg_touch_window_start_ms;
        if (elapsed_ms > sg_thresholds.touch_event_window_ms) {
            sg_touch_event_count = 0;
            sg_touch_window_start_ms = 0;
        }
    }

    if (touch_active && !sg_touch_prev_active) {
        if (sg_touch_event_count == 0) {
            sg_touch_window_start_ms = now_ms;
            sg_touch_event_count = 1;
        } else {
            elapsed_ms = now_ms - sg_touch_window_start_ms;
            if (elapsed_ms <= sg_thresholds.touch_event_window_ms) {
                if (sg_touch_event_count < 255U) {
                    sg_touch_event_count++;
                }
            } else {
                sg_touch_window_start_ms = now_ms;
                sg_touch_event_count = 1;
            }
        }
    }

    sg_touch_prev_active = touch_active ? 1U : 0U;

    if (sg_touch_event_count >= sg_thresholds.touch_min_events_in_window) {
        elapsed_ms = now_ms - sg_touch_window_start_ms;
        if (elapsed_ms <= sg_thresholds.touch_event_window_ms) {
            touch_threshold = 1;
        } else {
            sg_touch_event_count = 0;
            sg_touch_window_start_ms = 0;
        }
    }

    sg_touch_detail.touch_active_mask = touch_active_mask;
    sg_touch_detail.touch_rising_mask = touch_rising_mask;
    sg_touch_detail.touch_threshold = touch_threshold;
    sg_touch_detail.touch_threshold_rising = (touch_threshold && !sg_touch_prev_threshold) ? 1U : 0U;
    sg_touch_prev_threshold = touch_threshold;
    return touch_threshold;
}

static uint8_t __eval_imu(void)
{
    IMU_AHRS_STATUS_T status = {0};
    bool calibrated_ok = true;
    float accel_delta = 0.0f;
    float gx_abs = 0.0f;
    float gy_abs = 0.0f;
    float gz_abs = 0.0f;
    bool accel_motion = false;
    bool gyro_motion = false;
    bool norm_in_range = false;

    if (sg_hw == NULL) {
        return 0;
    }
    if (sg_hw->input.imu_get_status == NULL) {
        return 0;
    }
    if (sg_hw->input.imu_get_status(&status) != OPRT_OK) {
        return 0;
    }

    if (sg_thresholds.imu_require_calibrated) {
        calibrated_ok = status.calibrated;
    }

    if (!calibrated_ok) {
        sg_imu_motion_hits = 0;
        return 0;
    }

    norm_in_range = (status.accel_norm_mps2 >= sg_thresholds.imu_accel_norm_min_mps2 &&
                     status.accel_norm_mps2 <= sg_thresholds.imu_accel_norm_max_mps2);
    accel_delta = __float_abs(status.accel_norm_mps2 - INPUT_TRIGGER_GRAVITY_MPS2);
    gx_abs = __float_abs(status.gx_dps);
    gy_abs = __float_abs(status.gy_dps);
    gz_abs = __float_abs(status.gz_dps);
    accel_motion = (accel_delta >= sg_thresholds.imu_motion_accel_delta_min_mps2);
    gyro_motion = (gx_abs >= sg_thresholds.imu_motion_gyro_axis_min_dps ||
                   gy_abs >= sg_thresholds.imu_motion_gyro_axis_min_dps ||
                   gz_abs >= sg_thresholds.imu_motion_gyro_axis_min_dps);

    if (norm_in_range && (accel_motion || gyro_motion)) {
        if (sg_imu_motion_hits < 255U) {
            sg_imu_motion_hits++;
        }
    } else {
        sg_imu_motion_hits = 0;
    }

    if (sg_imu_motion_hits >= sg_thresholds.imu_motion_min_consecutive_hits) {
        return 1;
    }
    return 0;
}

OPERATE_RET input_trigger_layer_init(const INTERACTIVE_HARDWARE_ABSTRACTION_T *hardware_abstraction)
{
    sg_hw = hardware_abstraction;
    if (sg_hw == NULL) {
        return OPRT_INVALID_PARM;
    }

    __set_default_thresholds(&sg_thresholds);
    memset(&sg_flags, 0, sizeof(sg_flags));
    memset(&sg_touch_detail, 0, sizeof(sg_touch_detail));
    sg_imu_motion_hits = 0;
    sg_touch_prev_active = 0;
    sg_touch_event_count = 0;
    sg_touch_window_start_ms = 0;
    sg_touch_prev_mask = 0;
    sg_touch_prev_threshold = 0;
    return OPRT_OK;
}

OPERATE_RET input_trigger_layer_set_thresholds(const INPUT_TRIGGER_THRESHOLDS_T *thresholds)
{
    if (thresholds == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (thresholds->touch_min_active_channels > INPUT_TRIGGER_TOUCH_CH_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (thresholds->touch_event_window_ms == 0) {
        return OPRT_INVALID_PARM;
    }
    if (thresholds->touch_min_events_in_window == 0) {
        return OPRT_INVALID_PARM;
    }
    if (thresholds->imu_accel_norm_min_mps2 > thresholds->imu_accel_norm_max_mps2) {
        return OPRT_INVALID_PARM;
    }
    if (thresholds->imu_motion_min_consecutive_hits == 0) {
        return OPRT_INVALID_PARM;
    }

    sg_thresholds = *thresholds;
    sg_imu_motion_hits = 0;
    sg_touch_prev_active = 0;
    sg_touch_event_count = 0;
    sg_touch_window_start_ms = 0;
    sg_touch_prev_mask = 0;
    sg_touch_prev_threshold = 0;
    memset(&sg_touch_detail, 0, sizeof(sg_touch_detail));
    return OPRT_OK;
}

OPERATE_RET input_trigger_layer_eval(void)
{
    if (sg_hw == NULL) {
        return OPRT_COM_ERROR;
    }

    sg_flags.microphone = __eval_microphone();
    sg_flags.touch = __eval_touch();
    sg_flags.imu = __eval_imu();
    return OPRT_OK;
}

OPERATE_RET input_trigger_layer_get_flags(INPUT_TRIGGER_FLAGS_T *flags)
{
    if (flags == NULL) {
        return OPRT_INVALID_PARM;
    }
    *flags = sg_flags;
    return OPRT_OK;
}

OPERATE_RET input_trigger_layer_get_touch_detail(INPUT_TRIGGER_TOUCH_DETAIL_T *detail)
{
    if (detail == NULL) {
        return OPRT_INVALID_PARM;
    }
    *detail = sg_touch_detail;
    return OPRT_OK;
}

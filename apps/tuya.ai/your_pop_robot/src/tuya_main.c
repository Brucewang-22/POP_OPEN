/**
 * @file tuya_main.c
 * @brief Implements main audio functionality for IoT device
 *
 * This source file provides the implementation of the main audio functionalities
 * required for an IoT device. It includes functionality for audio processing,
 * device initialization, event handling, and network communication. The
 * implementation supports audio volume control, data point processing, and
 * interaction with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require audio capabilities and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include <string.h>

#include "cJSON.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tal_cli.h"
#include "hardware_abstraction.h"
#include "input_trigger_layer.h"
#include "behavior_pipeline.h"

#if defined(BOARD_CHOICE_LIBTECH_POP_T5AI_BOARD) && (BOARD_CHOICE_LIBTECH_POP_T5AI_BOARD == 1)
#include "board_com_api.h"
#else
static OPERATE_RET board_register_hardware(void)
{
    return OPRT_OK;
}
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define PRINTF_FREE_HEAP_TTIME (10 * 1000)
#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
#define IMU_POLL_TIME          (5)
#define IMU_STATUS_TIME        (20)
#endif
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
#define MIC_STATUS_TIME        (50)
#endif
#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
#define TOUCH_POLL_TIME        (50)
#define TOUCH_CH_CNT           (3)
#endif
#if ((defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)) || \
     (defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)) ||          \
     (defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)))
#define INPUT_TRIGGER_EVAL_TIME (50)
#endif

static TIMER_ID sg_printf_heap_tm;
static const INTERACTIVE_HARDWARE_ABSTRACTION_T *sg_hw_abstraction = NULL;
#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
static TIMER_ID sg_imu_poll_tm;
static TIMER_ID sg_imu_status_tm;
#endif
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
static TIMER_ID sg_mic_status_tm;
#endif
#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
static TIMER_ID sg_touch_poll_tm;
static uint8_t sg_touch_last_mask;
#endif
#if ((defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)) || \
     (defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)) ||          \
     (defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)))
static TIMER_ID sg_input_trigger_tm;
static INPUT_TRIGGER_FLAGS_T sg_input_trigger_last_flags;
#endif

static void __local_printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    uint32_t free_heap       = tal_system_get_free_heap_size();
    uint32_t free_psram_heap = tal_psram_get_free_heap_size();
    PR_INFO("Free heap size:%d, Free psram heap size:%d", free_heap, free_psram_heap);
#else
    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
#endif
}

#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
static void __local_mic_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint32_t frames_1s = 0;
    uint32_t rms_permille = 0;
    const char *status = "NO_DATA";

    (void)timer_id;
    (void)arg;

    if (sg_hw_abstraction->input.microphone_get_status(&frames_1s, &rms_permille) != OPRT_OK) {
        return;
    }
    if (frames_1s > 0) {
        status = "OK";
    }
    PR_NOTICE("[MIC] frames_1s=%u rms=%u status=%s", (unsigned)frames_1s, (unsigned)rms_permille, status);
}
#endif

#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
static void __local_touch_poll_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint8_t mask = 0;
    bool touched = false;

    (void)timer_id;
    (void)arg;

    for (uint8_t ch = 0; ch < TOUCH_CH_CNT; ch++) {
        if (sg_hw_abstraction->input.touch_read_channel(ch, &touched) != OPRT_OK) {
            continue;
        }
        if (touched) {
            mask |= (1U << ch);
        }
    }

    if (mask != sg_touch_last_mask) {
        for (uint8_t ch = 0; ch < TOUCH_CH_CNT; ch++) {
            uint8_t bit = (1U << ch);
            if ((mask & bit) == (sg_touch_last_mask & bit)) {
                continue;
            }
            PR_NOTICE("[TOUCH] ch=%u pin=%d touched=%u", (unsigned)ch,
                      (int)sg_hw_abstraction->input.touch_get_pin_channel(ch),
                      (unsigned)((mask & bit) ? 1 : 0));
        }
        sg_touch_last_mask = mask;
    }
}
#endif

#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
static void __local_imu_poll_tm_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    sg_hw_abstraction->input.imu_poll();
}

static void __local_imu_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    IMU_AHRS_STATUS_T status;

    (void)timer_id;
    (void)arg;
    if (sg_hw_abstraction->input.imu_get_status(&status) != OPRT_OK) {
        return;
    }
    PR_NOTICE("[IMU] who=0x%02X roll=%.2f pitch=%.2f yaw=%.2f temp=%.2f samples=%u calibrated=%u ax=%.4f ay=%.4f az=%.4f anorm=%.4f gx=%.3f gy=%.3f gz=%.3f",
              status.who_am_i, (double)status.roll_deg, (double)status.pitch_deg, (double)status.yaw_deg,
              (double)status.temp_c, (unsigned)status.sample_count, status.calibrated ? 1u : 0u,
              (double)status.ax_mps2, (double)status.ay_mps2, (double)status.az_mps2,
              (double)status.accel_norm_mps2, (double)status.gx_dps, (double)status.gy_dps, (double)status.gz_dps);
}
#endif

#if ((defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)) || \
     (defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)) ||          \
     (defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)))
static void __local_input_trigger_eval_tm_cb(TIMER_ID timer_id, void *arg)
{
    INPUT_TRIGGER_FLAGS_T flags = {0};

    (void)timer_id;
    (void)arg;

    if (input_trigger_layer_eval() != OPRT_OK) {
        return;
    }
    if (input_trigger_layer_get_flags(&flags) != OPRT_OK) {
        return;
    }

    if (flags.microphone != sg_input_trigger_last_flags.microphone ||
        flags.touch != sg_input_trigger_last_flags.touch ||
        flags.imu != sg_input_trigger_last_flags.imu) {
        PR_NOTICE("[INPUT_TRIGGER] mic=%u touch=%u imu=%u",
                  (unsigned)flags.microphone, (unsigned)flags.touch, (unsigned)flags.imu);
        sg_input_trigger_last_flags = flags;
    }

    (void)behavior_pipeline_interaction_phase();
}
#endif

#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
static OPERATE_RET __local_lcd_basic_init(void)
{
    OPERATE_RET left_rt = OPRT_OK;
    OPERATE_RET right_rt = OPRT_OK;

    left_rt = sg_hw_abstraction->output.lcd_clear(LCD_INSTANCE_0);
    right_rt = sg_hw_abstraction->output.lcd_clear(LCD_INSTANCE_1);

    if (left_rt != OPRT_OK && right_rt != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("[LOCAL][LCD] basic init done, display0=%d display1=%d", left_rt, right_rt);
    return OPRT_OK;
}
#endif

static OPERATE_RET app_local_hw_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    sg_hw_abstraction = interactive_get_hardware_abstraction();
    TUYA_CHECK_NULL_RETURN(sg_hw_abstraction, OPRT_COM_ERROR);

    PR_NOTICE("[LOCAL] hardware-only init start");

#if ((defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)) || \
     (defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)) ||          \
     (defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)))
    TUYA_CALL_ERR_LOG(input_trigger_layer_init(sg_hw_abstraction));
    memset(&sg_input_trigger_last_flags, 0, sizeof(sg_input_trigger_last_flags));
#endif

    tal_sw_timer_create(__local_printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
    rt = sg_hw_abstraction->input.imu_init();
    if (rt == OPRT_OK) {
        tal_sw_timer_create(__local_imu_poll_tm_cb, NULL, &sg_imu_poll_tm);
        tal_sw_timer_start(sg_imu_poll_tm, IMU_POLL_TIME, TAL_TIMER_CYCLE);
        tal_sw_timer_create(__local_imu_status_tm_cb, NULL, &sg_imu_status_tm);
        tal_sw_timer_start(sg_imu_status_tm, IMU_STATUS_TIME, TAL_TIMER_CYCLE);
        PR_NOTICE("[LOCAL][IMU] init ok");
    } else {
        PR_WARN("[LOCAL][IMU] init failed: %d (optional)", rt);
    }
#endif

#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
    TUYA_CALL_ERR_LOG(sg_hw_abstraction->input.microphone_init());
    tal_sw_timer_create(__local_mic_status_tm_cb, NULL, &sg_mic_status_tm);
    tal_sw_timer_start(sg_mic_status_tm, MIC_STATUS_TIME, TAL_TIMER_CYCLE);
    PR_WARN("[LOCAL][MIC] AI pipeline disabled in local mode, mic frames may remain zero");
#endif

#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
    rt = sg_hw_abstraction->output.moto_init();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][MOTO] init failed: %d (optional)", rt);
    } else {
        PR_NOTICE("[LOCAL][MOTO] init ok");
    }
#endif

#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
    {
        INTERACTIVE_TOUCH_CHANNEL_CFG_T cfgs[TOUCH_CH_CNT] = {
            {.gpio_pin = TUYA_GPIO_NUM_26, .active_level = TUYA_GPIO_LEVEL_LOW, .pull = TUYA_GPIO_PULLUP},
            {.gpio_pin = TUYA_GPIO_NUM_22, .active_level = TUYA_GPIO_LEVEL_LOW, .pull = TUYA_GPIO_PULLUP},
            {.gpio_pin = TUYA_GPIO_NUM_23, .active_level = TUYA_GPIO_LEVEL_LOW, .pull = TUYA_GPIO_PULLUP},
        };
        bool touch_enabled = true;

        sg_touch_last_mask = 0;
        for (uint8_t ch = 0; ch < TOUCH_CH_CNT; ch++) {
            rt = sg_hw_abstraction->input.touch_init_channel(ch, &cfgs[ch]);
            if (rt != OPRT_OK) {
                PR_WARN("[LOCAL][TOUCH] init failed: ch=%u rt=%d (optional)", (unsigned)ch, rt);
                touch_enabled = false;
                break;
            }
        }

        if (touch_enabled) {
            tal_sw_timer_create(__local_touch_poll_tm_cb, NULL, &sg_touch_poll_tm);
            tal_sw_timer_start(sg_touch_poll_tm, TOUCH_POLL_TIME, TAL_TIMER_CYCLE);
            PR_NOTICE("[LOCAL][TOUCH] init ok, polling %ums on P26/P22/P23", TOUCH_POLL_TIME);
        }
    }
#endif

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    PR_NOTICE("[LOCAL][AUDIO] playback test disabled in local init");
#endif

#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
    rt = __local_lcd_basic_init();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][LCD] basic init failed: %d (optional)", rt);
    }
#endif

#if ((defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)) || \
     (defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)) ||          \
     (defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)))
    tal_sw_timer_create(__local_input_trigger_eval_tm_cb, NULL, &sg_input_trigger_tm);
    tal_sw_timer_start(sg_input_trigger_tm, INPUT_TRIGGER_EVAL_TIME, TAL_TIMER_CYCLE);
#endif

    rt = behavior_pipeline_init_phase();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][BEHAVIOR] init phase failed: %d (optional)", rt);
    }

    PR_NOTICE("[LOCAL] hardware-only init done");
    return OPRT_OK;
}


void user_main(void)
{
    int ret = OPRT_OK;

    //! open iot development kit runtim init
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_psram_malloc, .free_fn = tal_psram_free});
#else 
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
#endif

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_time_service_init();
    tal_cli_init();

    ret = board_register_hardware();
    if (ret != OPRT_OK) {
        PR_ERR("board_register_hardware failed");
    }

    PR_NOTICE("Local hardware-only mode enabled: cloud/netcfg is fully disabled");
    ret = app_local_hw_init();
    if (ret != OPRT_OK) {
        PR_ERR("app_local_hw_init failed: %d", ret);
    }

    for (;;) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 4096;
    thrd_param.priority = 4;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

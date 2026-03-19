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
#include "tdl_display_manage.h"
#include "tkl_jpeg_codec.h"

#if defined(BOARD_CHOICE_LIBTECH_POP_T5AI_BOARD) && (BOARD_CHOICE_LIBTECH_POP_T5AI_BOARD == 1)
#include "board_com_api.h"
#else
static OPERATE_RET board_register_hardware(void)
{
    return OPRT_OK;
}
#endif


#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
#include "imu_ahrs.h"
#endif
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
#include "mic_bringup.h"
#endif
#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
#include "moto_bringup.h"
#endif
#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
#include "touch_sensor.h"
#endif
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "local_audio.h"
#endif

#include "lcd_asset_types.h"
#include "left_eye_lcd_assets.h"
#include "right_eye_lcd_assets.h"

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
#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
#define LOCAL_LCD_ANIM_INTERVAL_MS (40)
#define LOCAL_LCD_BUF_MAX_SIZE     (160U * 160U * 2U)
#define LOCAL_LCD_FLUSH_WAIT_MS    (200)
#endif

static TIMER_ID sg_printf_heap_tm;
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
#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
static THREAD_HANDLE sg_lcd_anim_thread = NULL;
static TDL_DISP_HANDLE_T sg_lcd_left_disp = NULL;
static TDL_DISP_HANDLE_T sg_lcd_right_disp = NULL;
static TDL_DISP_DEV_INFO_T sg_lcd_left_info;
static TDL_DISP_DEV_INFO_T sg_lcd_right_info;
static uint8_t *sg_lcd_left_decode_buf = NULL;
static uint8_t *sg_lcd_right_decode_buf = NULL;
static uint32_t sg_lcd_decode_buf_size = 0;
static SEM_HANDLE sg_lcd_left_flush_sem = NULL;
static SEM_HANDLE sg_lcd_right_flush_sem = NULL;
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

    if (mic_bringup_get_status(&frames_1s, &rms_permille) != OPRT_OK) {
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
        if (touch_sensor_read_channel(ch, &touched) != OPRT_OK) {
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
            PR_NOTICE("[TOUCH] ch=%u pin=%d touched=%u", (unsigned)ch, (int)touch_sensor_get_pin_channel(ch),
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
    imu_ahrs_poll();
}

static void __local_imu_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    IMU_AHRS_STATUS_T status;

    (void)timer_id;
    (void)arg;
    if (imu_ahrs_get_status(&status) != OPRT_OK) {
        return;
    }
    PR_NOTICE("[IMU] who=0x%02X roll=%.2f pitch=%.2f yaw=%.2f temp=%.2f samples=%u calibrated=%u ax=%.4f ay=%.4f az=%.4f anorm=%.4f gx=%.3f gy=%.3f gz=%.3f",
              status.who_am_i, (double)status.roll_deg, (double)status.pitch_deg, (double)status.yaw_deg,
              (double)status.temp_c, (unsigned)status.sample_count, status.calibrated ? 1u : 0u,
              (double)status.ax_mps2, (double)status.ay_mps2, (double)status.az_mps2,
              (double)status.accel_norm_mps2, (double)status.gx_dps, (double)status.gy_dps, (double)status.gz_dps);
}
#endif

#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
static OPERATE_RET __local_lcd_open_one(const char *name, TDL_DISP_HANDLE_T *out_disp, TDL_DISP_DEV_INFO_T *out_info)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_HANDLE_T disp = NULL;

    disp = tdl_disp_find_dev((char *)name);
    if (disp == NULL) {
        PR_WARN("[LOCAL][LCD] display '%s' not found", name);
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(disp));
    TUYA_CALL_ERR_LOG(tdl_disp_set_brightness(disp, 100));
    TUYA_CALL_ERR_RETURN(tdl_disp_dev_get_info(disp, out_info));
    *out_disp = disp;

    PR_NOTICE("[LOCAL][LCD] open %s ok: %ux%u fmt=%d swap=%u", name,
              (unsigned)out_info->width, (unsigned)out_info->height,
              (int)out_info->fmt, (unsigned)(out_info->is_swap ? 1 : 0));

    return rt;
}

static void __local_lcd_flush_done_cb(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    if (frame_buff == NULL || frame_buff->free_arg == NULL) {
        return;
    }

    tal_semaphore_post((SEM_HANDLE)frame_buff->free_arg);
}

static OPERATE_RET __local_lcd_render_one(TDL_DISP_HANDLE_T disp, TDL_DISP_DEV_INFO_T *disp_info,
                                          uint8_t *decode_buf, uint32_t decode_buf_size, SEM_HANDLE flush_sem,
                                          const uint8_t *jpg_data, uint32_t jpg_len, const char *name)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_JPEG_CODEC_INFO_T jpeg_info = {0};
    TDL_DISP_FRAME_BUFF_T fb = {0};
    uint32_t need_size = 0;
    uint32_t pixel_cnt = 0;

    if (disp == NULL || disp_info == NULL || decode_buf == NULL || jpg_data == NULL || jpg_len == 0) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_jpeg_codec_img_info_get((uint8_t *)jpg_data, jpg_len, &jpeg_info));

    need_size = (uint32_t)jpeg_info.out_width * (uint32_t)jpeg_info.out_height * 2U;
    if (need_size > decode_buf_size) {
        PR_ERR("[LOCAL][LCD] frame %s oversize: need=%u buf=%u", name, (unsigned)need_size,
               (unsigned)decode_buf_size);
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(
        tkl_jpeg_codec_convert((uint8_t *)jpg_data, decode_buf, &jpeg_info, JPEG_DEC_OUT_RGB565));

    fb.fmt = TUYA_PIXEL_FMT_RGB565;
    fb.x_start = 0;
    fb.y_start = 0;
    fb.width = jpeg_info.out_width;
    fb.height = jpeg_info.out_height;
    fb.len = need_size;
    fb.frame = decode_buf;
    fb.free_cb = __local_lcd_flush_done_cb;
    fb.free_arg = flush_sem;

    pixel_cnt = (uint32_t)jpeg_info.out_width * (uint32_t)jpeg_info.out_height;
    if (disp_info->is_swap) {
        TUYA_CALL_ERR_LOG(tdl_disp_dev_rgb565_swap((uint16_t *)decode_buf, pixel_cnt));
    }
    TUYA_CALL_ERR_LOG(tdl_disp_dev_flush(disp, &fb));
    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(flush_sem, LOCAL_LCD_FLUSH_WAIT_MS));

    return rt;
}

static void __local_lcd_anim_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t left_idx = 0;
    uint32_t right_idx = 0;

    (void)arg;
    if (g_left_eye_lcd_frame_count == 0 || g_right_eye_lcd_frame_count == 0) {
        PR_WARN("[LOCAL][LCD] left/right eye frame asset missing");
        return;
    }

    PR_NOTICE("[LOCAL][LCD] dual animation loop start, left=%u right=%u interval=%ums",
              (unsigned)g_left_eye_lcd_frame_count, (unsigned)g_right_eye_lcd_frame_count,
              (unsigned)LOCAL_LCD_ANIM_INTERVAL_MS);

    while (1) {
        const LOCAL_LCD_FRAME_T *left_frame = &g_left_eye_lcd_frames[left_idx];
        const LOCAL_LCD_FRAME_T *right_frame = &g_right_eye_lcd_frames[right_idx];

        if (sg_lcd_left_disp != NULL) {
            TUYA_CALL_ERR_LOG(__local_lcd_render_one(
                sg_lcd_left_disp, &sg_lcd_left_info, sg_lcd_left_decode_buf, sg_lcd_decode_buf_size,
                sg_lcd_left_flush_sem,
                left_frame->data, left_frame->len, left_frame->name));
        }
        if (sg_lcd_right_disp != NULL) {
            TUYA_CALL_ERR_LOG(__local_lcd_render_one(
                sg_lcd_right_disp, &sg_lcd_right_info, sg_lcd_right_decode_buf, sg_lcd_decode_buf_size,
                sg_lcd_right_flush_sem,
                right_frame->data, right_frame->len, right_frame->name));
        }

        left_idx++;
        if (left_idx >= g_left_eye_lcd_frame_count) {
            left_idx = 0;
        }
        right_idx++;
        if (right_idx >= g_right_eye_lcd_frame_count) {
            right_idx = 0;
        }
        tal_system_sleep(LOCAL_LCD_ANIM_INTERVAL_MS);
    }
}

static OPERATE_RET __local_lcd_anim_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_CFG_T cfg = {0};

    TUYA_CALL_ERR_LOG(__local_lcd_open_one("display", &sg_lcd_left_disp, &sg_lcd_left_info));
    TUYA_CALL_ERR_LOG(__local_lcd_open_one("display2", &sg_lcd_right_disp, &sg_lcd_right_info));

    if (sg_lcd_left_disp == NULL && sg_lcd_right_disp == NULL) {
        PR_WARN("[LOCAL][LCD] no display opened for dual-eye mode");
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(tkl_jpeg_codec_init());

    sg_lcd_decode_buf_size = LOCAL_LCD_BUF_MAX_SIZE;
    sg_lcd_left_decode_buf = (uint8_t *)tal_malloc(sg_lcd_decode_buf_size);
    sg_lcd_right_decode_buf = (uint8_t *)tal_malloc(sg_lcd_decode_buf_size);
    TUYA_CHECK_NULL_RETURN(sg_lcd_left_decode_buf, OPRT_MALLOC_FAILED);
    TUYA_CHECK_NULL_RETURN(sg_lcd_right_decode_buf, OPRT_MALLOC_FAILED);
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_lcd_left_flush_sem, 0, 1));
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_lcd_right_flush_sem, 0, 1));
    memset(sg_lcd_left_decode_buf, 0, sg_lcd_decode_buf_size);
    memset(sg_lcd_right_decode_buf, 0, sg_lcd_decode_buf_size);

    cfg.stackDepth = 4096;
    cfg.priority = THREAD_PRIO_2;
    cfg.thrdname = "lcd_anim";
    TUYA_CALL_ERR_RETURN(
        tal_thread_create_and_start(&sg_lcd_anim_thread, NULL, NULL, __local_lcd_anim_task, NULL, &cfg));

    PR_NOTICE("[LOCAL][LCD] dual-eye init ok, left=%u right=%u decode_buf=%u bytes",
              (unsigned)(sg_lcd_left_disp != NULL ? 1 : 0),
              (unsigned)(sg_lcd_right_disp != NULL ? 1 : 0),
              (unsigned)sg_lcd_decode_buf_size);
    return rt;
}
#endif

static OPERATE_RET app_local_hw_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("[LOCAL] hardware-only init start");

    tal_sw_timer_create(__local_printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
    rt = imu_ahrs_init();
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
    TUYA_CALL_ERR_LOG(mic_bringup_init());
    tal_sw_timer_create(__local_mic_status_tm_cb, NULL, &sg_mic_status_tm);
    tal_sw_timer_start(sg_mic_status_tm, MIC_STATUS_TIME, TAL_TIMER_CYCLE);
    PR_WARN("[LOCAL][MIC] AI pipeline disabled in local mode, mic frames may remain zero");
#endif

#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
    rt = moto_bringup_init();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][MOTO] init failed: %d (optional)", rt);
    } else {
        PR_NOTICE("[LOCAL][MOTO] init ok");
    }
#endif

#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
    {
        TOUCH_SENSOR_CFG_T cfgs[TOUCH_CH_CNT] = {
            {.gpio_pin = TUYA_GPIO_NUM_26, .active_level = TUYA_GPIO_LEVEL_HIGH, .pull = TUYA_GPIO_PULLDOWN},
            {.gpio_pin = TUYA_GPIO_NUM_22, .active_level = TUYA_GPIO_LEVEL_HIGH, .pull = TUYA_GPIO_PULLDOWN},
            {.gpio_pin = TUYA_GPIO_NUM_23, .active_level = TUYA_GPIO_LEVEL_HIGH, .pull = TUYA_GPIO_PULLDOWN},
        };
        bool touch_enabled = true;

        sg_touch_last_mask = 0;
        for (uint8_t ch = 0; ch < TOUCH_CH_CNT; ch++) {
            rt = touch_sensor_init_channel(ch, &cfgs[ch]);
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
    rt = local_audio_start_test();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][AUDIO] init/play failed: %d (optional)", rt);
    }
#endif

#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
    rt = __local_lcd_anim_init();
    if (rt != OPRT_OK) {
        PR_WARN("[LOCAL][LCD] animation init failed: %d (optional)", rt);
    }
#endif

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

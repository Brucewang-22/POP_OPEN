/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "tal_api.h"

#include "netmgr.h"

#include "ai_chat_main.h"
#include "app_chat_bot.h"
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

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define PRINTF_FREE_HEAP_TTIME (10 * 1000)
#define DISP_NET_STATUS_TIME   (1 * 1000)
/* 与 POP_Open/demo/hardware IMU 一致：解算频率拉高，状态输出完整 */
#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
#define IMU_POLL_TIME          (5)   /* 5ms = 200Hz 解算 */
#define IMU_STATUS_TIME        (20)  /* 20ms = 50Hz 串口输出 */
#endif
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
#define MIC_STATUS_TIME        (50)  /* 50ms = 20Hz 串口输出 */
#endif
#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
#define TOUCH_POLL_TIME        (50)  /* 50ms 轮询 */
#define TOUCH_CH_CNT           (3)
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************const declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID sg_printf_heap_tm;

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static AI_UI_WIFI_STATUS_E sg_wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
static TIMER_ID            sg_disp_status_tm;
#endif

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

/***********************************************************
***********************function define**********************
***********************************************************/
static void __printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
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

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static void __display_net_status_update(void)
{
    AI_UI_WIFI_STATUS_E wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e     net_status  = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
        // get rssi
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        // BUG: Getting RSSI causes a crash on T5 platform
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        if (rssi >= -60) {
            wifi_status = AI_UI_WIFI_STATUS_GOOD;
        } else if (rssi >= -70) {
            wifi_status = AI_UI_WIFI_STATUS_FAIR;
        } else {
            wifi_status = AI_UI_WIFI_STATUS_WEAK;
        }
#else
        wifi_status = AI_UI_WIFI_STATUS_GOOD;
#endif
    } else {
        wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    }

    if (wifi_status != sg_wifi_status) {
        sg_wifi_status = wifi_status;
        ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&wifi_status, sizeof(AI_UI_WIFI_STATUS_E));
    }
}

static void __display_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    __display_net_status_update();
}

#endif

#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
static void __mic_status_tm_cb(TIMER_ID timer_id, void *arg)
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
static void __touch_poll_tm_cb(TIMER_ID timer_id, void *arg)
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
static void __imu_poll_tm_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    imu_ahrs_poll();
}

static void __imu_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    IMU_AHRS_STATUS_T status;

    (void)timer_id;
    (void)arg;
    if (imu_ahrs_get_status(&status) != OPRT_OK) {
        return;
    }
    /* 与 POP_Open/demo/hardware IMU 输出一一对应：who roll pitch yaw temp samples calibrated ax ay az anorm gx gy gz */
    PR_NOTICE("[IMU] who=0x%02X roll=%.2f pitch=%.2f yaw=%.2f temp=%.2f samples=%u calibrated=%u ax=%.4f ay=%.4f az=%.4f anorm=%.4f gx=%.3f gy=%.3f gz=%.3f",
              status.who_am_i, (double)status.roll_deg, (double)status.pitch_deg, (double)status.yaw_deg,
              (double)status.temp_c, (unsigned)status.sample_count, status.calibrated ? 1u : 0u,
              (double)status.ax_mps2, (double)status.ay_mps2, (double)status.az_mps2,
              (double)status.accel_norm_mps2,
              (double)status.gx_dps, (double)status.gy_dps, (double)status.gz_dps);
}
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static void __ai_video_display_flush(TDL_CAMERA_FRAME_T *frame)
{
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_camera_flush(frame->data, frame->width, frame->height);
#endif
}
#endif

static void __ai_chat_handle_event(AI_NOTIFY_EVENT_T *event)
{
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
    mic_bringup_on_ai_event(event);
#endif
    (void)event;
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_WAKEUP,
        .default_vol  = 70,
        .evt_cb       = __ai_chat_handle_event,
    };
    TUYA_CALL_ERR_RETURN(ai_chat_init(&ai_chat_cfg));

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    AI_VIDEO_CFG_T ai_video_cfg = {
        .disp_flush_cb = __ai_video_display_flush,
    };

    TUYA_CALL_ERR_LOG(ai_video_init(&ai_video_cfg));
#endif

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
    TUYA_CALL_ERR_RETURN(ai_mcp_init());
#endif

    // Free heap size
    tal_sw_timer_create(__printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&sg_wifi_status, sizeof(AI_UI_WIFI_STATUS_E));

    ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)INITIALIZING, strlen(INITIALIZING));
    ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));

    // display status update
    tal_sw_timer_create(__display_status_tm_cb, NULL, &sg_disp_status_tm);
    tal_sw_timer_start(sg_disp_status_tm, DISP_NET_STATUS_TIME, TAL_TIMER_CYCLE);
#endif

#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
    rt = imu_ahrs_init();
    if (rt == OPRT_OK) {
        tal_sw_timer_create(__imu_poll_tm_cb, NULL, &sg_imu_poll_tm);
        tal_sw_timer_start(sg_imu_poll_tm, IMU_POLL_TIME, TAL_TIMER_CYCLE);
        tal_sw_timer_create(__imu_status_tm_cb, NULL, &sg_imu_status_tm);
        tal_sw_timer_start(sg_imu_status_tm, IMU_STATUS_TIME, TAL_TIMER_CYCLE);
    } else {
        PR_WARN("imu_ahrs_init failed: %d (IMU optional)", rt);
    }
#endif

#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
    TUYA_CALL_ERR_LOG(mic_bringup_init());
    tal_sw_timer_create(__mic_status_tm_cb, NULL, &sg_mic_status_tm);
    tal_sw_timer_start(sg_mic_status_tm, MIC_STATUS_TIME, TAL_TIMER_CYCLE);
#endif

#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
    rt = moto_bringup_init();
    if (rt != OPRT_OK) {
        PR_WARN("moto_bringup_init failed: %d (MOTO optional)", rt);
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
                PR_WARN("touch_sensor_init_channel failed: ch=%u rt=%d (TOUCH optional)", (unsigned)ch, rt);
                touch_enabled = false;
                break;
            }
        }

        if (touch_enabled) {
            tal_sw_timer_create(__touch_poll_tm_cb, NULL, &sg_touch_poll_tm);
            tal_sw_timer_start(sg_touch_poll_tm, TOUCH_POLL_TIME, TAL_TIMER_CYCLE);
            PR_NOTICE("[TOUCH] init ok, polling %ums on P26/P22/P23", TOUCH_POLL_TIME);
        }
    }
#endif

    return OPRT_OK;
}

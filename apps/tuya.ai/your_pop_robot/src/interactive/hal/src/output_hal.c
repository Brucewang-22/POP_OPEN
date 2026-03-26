/**
 * @file output_hal.c
 * @brief Output HAL mode control (lcd/audio/moto).
 */

#include "output_hal.h"

#include "tal_api.h"
#include "tdl_display_manage.h"
#include "tkl_jpeg_codec.h"
#include "tkl_gpio.h"
#include "tkl_pwm.h"

#include "ai_audio_player.h"
#include "svc_ai_player.h"
#include "tdl_audio_manage.h"
#include "output_mode_config.h"

#include "moto_bringup.h"
#include "lcd_asset_types.h"
#include "left_eye_doubt_lcd_assets.h"
#include "left_eye_happy_lcd_assets.h"
#include "left_eye_normal_lcd_assets.h"
#include "right_eye_doubt_lcd_assets.h"
#include "right_eye_happy_lcd_assets.h"
#include "right_eye_normal_lcd_assets.h"
#include "doubt_audio_assets.h"

extern const LOCAL_AUDIO_CLIP_T g_happy_audio_clips[];
extern const uint32_t g_happy_audio_clip_count;
extern const LOCAL_AUDIO_CLIP_T g_normal_audio_clips[];
extern const uint32_t g_normal_audio_clip_count;

#define OUT_LCD_BUF_MAX_SIZE     (160U * 160U * 2U)
#define OUT_LCD_FLUSH_WAIT_MS    200U
#define OUT_AUDIO_CHUNK_SIZE     2048U

#define OUT_MOTO_PWM_DUTY_MIN    250U
#define OUT_MOTO_PWM_DUTY_MAX    1250U
#define OUT_MOTO_STEP_PERIOD_MS  20U

typedef struct {
    const LOCAL_LCD_FRAME_T *left_frames;
    uint32_t left_count;
    const LOCAL_LCD_FRAME_T *right_frames;
    uint32_t right_count;
} OUT_LCD_MODE_SET_T;

typedef struct {
    const LOCAL_AUDIO_CLIP_T *clips;
    uint32_t count;
} OUT_AUDIO_MODE_SET_T;

typedef struct {
    uint32_t loop_time_ms;
    uint32_t fps;
} OUT_LCD_MODE_CFG_T;

typedef struct {
    uint32_t volume;
} OUT_AUDIO_MODE_CFG_T;

typedef struct {
    uint32_t angle_deg;
    uint32_t speed_dps;
    bool loop;
    bool hold_torque;
    uint32_t run_time_ms;
} OUT_MOTO_MODE_CFG_T;

static const OUT_LCD_MODE_CFG_T sg_lcd_cfg[] = {
    {HWCFG_LCD_MODEL1_LOOP_TIME_MS, HWCFG_LCD_MODEL1_FPS},
    {HWCFG_LCD_MODEL2_LOOP_TIME_MS, HWCFG_LCD_MODEL2_FPS},
    {HWCFG_LCD_MODEL3_LOOP_TIME_MS, HWCFG_LCD_MODEL3_FPS},
};

static const OUT_AUDIO_MODE_CFG_T sg_audio_cfg[] = {
    {HWCFG_AUDIO_MODEL1_VOLUME},
    {HWCFG_AUDIO_MODEL2_VOLUME},
    {HWCFG_AUDIO_MODEL3_VOLUME},
};

static const OUT_MOTO_MODE_CFG_T sg_moto_cfg[] = {
    {HWCFG_MOTO_MODEL1_ANGLE_DEG, HWCFG_MOTO_MODEL1_SPEED_DPS, HWCFG_MOTO_MODEL1_LOOP,
     HWCFG_MOTO_MODEL1_HOLD_TORQUE, HWCFG_MOTO_MODEL1_RUN_TIME_MS},
    {HWCFG_MOTO_MODEL2_ANGLE_DEG, HWCFG_MOTO_MODEL2_SPEED_DPS, HWCFG_MOTO_MODEL2_LOOP,
     HWCFG_MOTO_MODEL2_HOLD_TORQUE, HWCFG_MOTO_MODEL2_RUN_TIME_MS},
    {HWCFG_MOTO_MODEL3_ANGLE_DEG, HWCFG_MOTO_MODEL3_SPEED_DPS, HWCFG_MOTO_MODEL3_LOOP,
     HWCFG_MOTO_MODEL3_HOLD_TORQUE, HWCFG_MOTO_MODEL3_RUN_TIME_MS},
};

static bool sg_output_inited = false;
static bool sg_audio_ready = false;
static MUTEX_HANDLE sg_lcd_mutex = NULL;
static int16_t sg_moto_logical_offset[2] = {0, 0};

static void __lcd_flush_done_cb(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    if (frame_buff == NULL || frame_buff->free_arg == NULL) {
        return;
    }
    tal_semaphore_post((SEM_HANDLE)frame_buff->free_arg);
}

static OPERATE_RET __lcd_open(const char *name, TDL_DISP_HANDLE_T *disp, TDL_DISP_DEV_INFO_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    *disp = tdl_disp_find_dev((char *)name);
    if (*disp == NULL) {
        PR_ERR("[LCD] find dev failed: %s", name);
        return OPRT_COM_ERROR;
    }
    rt = tdl_disp_dev_open(*disp);
    if (rt != OPRT_OK) {
        PR_ERR("[LCD] open failed: %s rt=%d", name, rt);
        return rt;
    }
    rt = tdl_disp_dev_get_info(*disp, info);
    if (rt != OPRT_OK) {
        PR_ERR("[LCD] get info failed: %s rt=%d", name, rt);
        return rt;
    }
    /* tdl_disp_dev_open only inits BL pin, it does not turn BL on. */
    (void)tdl_disp_set_brightness(*disp, 100);
    PR_NOTICE("[LCD] open ok: %s %ux%u swap=%u", name, (unsigned)info->width, (unsigned)info->height,
              (unsigned)info->is_swap);
    return OPRT_OK;
}

static OPERATE_RET __lcd_render_frame(TDL_DISP_HANDLE_T disp, TDL_DISP_DEV_INFO_T *info,
                                      uint8_t *decode_buf, uint32_t decode_buf_size, SEM_HANDLE flush_sem,
                                      const LOCAL_LCD_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_JPEG_CODEC_INFO_T jpeg_info = {0};
    TDL_DISP_FRAME_BUFF_T fb = {0};
    uint32_t need_size = 0;
    uint32_t pixel_cnt = 0;

    if (disp == NULL || info == NULL || decode_buf == NULL || frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_jpeg_codec_img_info_get((uint8_t *)frame->data, frame->len, &jpeg_info);
    if (rt != OPRT_OK) {
        return rt;
    }
    need_size = (uint32_t)jpeg_info.out_width * (uint32_t)jpeg_info.out_height * 2U;
    if (need_size > decode_buf_size) {
        return OPRT_COM_ERROR;
    }

    rt = tkl_jpeg_codec_convert((uint8_t *)frame->data, decode_buf, &jpeg_info, JPEG_DEC_OUT_RGB565);
    if (rt != OPRT_OK) {
        return rt;
    }

    fb.fmt = TUYA_PIXEL_FMT_RGB565;
    fb.x_start = 0;
    fb.y_start = 0;
    fb.width = jpeg_info.out_width;
    fb.height = jpeg_info.out_height;
    fb.len = need_size;
    fb.frame = decode_buf;
    fb.free_cb = __lcd_flush_done_cb;
    fb.free_arg = flush_sem;

    pixel_cnt = (uint32_t)jpeg_info.out_width * (uint32_t)jpeg_info.out_height;
    if (info->is_swap) {
        TUYA_CALL_ERR_LOG(tdl_disp_dev_rgb565_swap((uint16_t *)decode_buf, pixel_cnt));
    }
    rt = tdl_disp_dev_flush(disp, &fb);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tal_semaphore_wait(flush_sem, OUT_LCD_FLUSH_WAIT_MS);
    if (rt != OPRT_OK) {
        return rt;
    }
    return rt;
}

static OPERATE_RET __lcd_play_mode(const OUT_LCD_MODE_SET_T *set, const OUT_LCD_MODE_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_HANDLE_T left_disp = NULL;
    TDL_DISP_HANDLE_T right_disp = NULL;
    TDL_DISP_DEV_INFO_T left_info = {0};
    TDL_DISP_DEV_INFO_T right_info = {0};
    uint8_t *left_buf = NULL;
    uint8_t *right_buf = NULL;
    SEM_HANDLE left_sem = NULL;
    SEM_HANDLE right_sem = NULL;
    uint32_t total = 0;
    uint32_t fail_cnt = 0;
    uint32_t frame_interval_ms = 40;
    uint32_t i = 0;
    uint64_t start_ms = 0;

    if (set == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }
    total = (set->left_count > set->right_count) ? set->left_count : set->right_count;
    if (total == 0) {
        return OPRT_INVALID_PARM;
    }

    rt = __lcd_open("display0", &left_disp, &left_info);
    if (rt != OPRT_OK) {
        left_disp = NULL;
    }
    rt = __lcd_open("display1", &right_disp, &right_info);
    if (rt != OPRT_OK) {
        right_disp = NULL;
    }
    if (left_disp == NULL && right_disp == NULL) {
        return OPRT_COM_ERROR;
    }

    rt = tkl_jpeg_codec_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    left_buf = (uint8_t *)tal_malloc(OUT_LCD_BUF_MAX_SIZE);
    right_buf = (uint8_t *)tal_malloc(OUT_LCD_BUF_MAX_SIZE);
    if (left_buf == NULL || right_buf == NULL) {
        rt = OPRT_MALLOC_FAILED;
        goto __exit;
    }
    rt = tal_semaphore_create_init(&left_sem, 0, 1);
    if (rt != OPRT_OK) {
        goto __exit;
    }
    rt = tal_semaphore_create_init(&right_sem, 0, 1);
    if (rt != OPRT_OK) {
        goto __exit;
    }

    if (cfg->fps > 0) {
        frame_interval_ms = 1000U / cfg->fps;
        if (frame_interval_ms == 0) {
            frame_interval_ms = 1;
        }
    }

    start_ms = tal_system_get_millisecond();
    while (1) {
        if (left_disp != NULL && set->left_count > 0) {
            const LOCAL_LCD_FRAME_T *f = &set->left_frames[i % set->left_count];
            rt = __lcd_render_frame(left_disp, &left_info, left_buf, OUT_LCD_BUF_MAX_SIZE, left_sem, f);
            if (rt != OPRT_OK) {
                PR_WARN("[LCD] left render failed: frame=%u rt=%d", (unsigned)i, rt);
                fail_cnt++;
            }
        }
        if (right_disp != NULL && set->right_count > 0) {
            const LOCAL_LCD_FRAME_T *f = &set->right_frames[i % set->right_count];
            rt = __lcd_render_frame(right_disp, &right_info, right_buf, OUT_LCD_BUF_MAX_SIZE, right_sem, f);
            if (rt != OPRT_OK) {
                PR_WARN("[LCD] right render failed: frame=%u rt=%d", (unsigned)i, rt);
                fail_cnt++;
            }
        }
        i++;
        tal_system_sleep(frame_interval_ms);

        if (cfg->loop_time_ms == 0) {
            if (i >= total) {
                break;
            }
        } else if ((tal_system_get_millisecond() - start_ms) >= cfg->loop_time_ms) {
            break;
        }
    }

    if (fail_cnt >= i) {
        rt = OPRT_NOT_SUPPORTED;
    }

    PR_NOTICE("[LCD] mode done: frames=%u fail=%u rt=%d", (unsigned)i, (unsigned)fail_cnt, rt);

__exit:
    if (left_sem != NULL) {
        (void)tal_semaphore_release(left_sem);
    }
    if (right_sem != NULL) {
        (void)tal_semaphore_release(right_sem);
    }
    (void)tkl_jpeg_codec_deinit();

    /* Keep display device opened to avoid backlight deinit right after init-phase playback. */
    if (left_buf != NULL) {
        tal_free(left_buf);
    }
    if (right_buf != NULL) {
        tal_free(right_buf);
    }
    return rt;
}

static OPERATE_RET __audio_prepare_once(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T spk_gpio_cfg = {0};
    TDL_AUDIO_HANDLE_T audio_hdl = NULL;

    if (sg_audio_ready) {
        return OPRT_OK;
    }

    rt = tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tdl_audio_open(audio_hdl, NULL);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = ai_audio_player_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)ai_audio_player_set_vol(70);

    spk_gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    spk_gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    spk_gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    (void)tkl_gpio_init(TUYA_GPIO_NUM_39, &spk_gpio_cfg);
    (void)tkl_gpio_write(TUYA_GPIO_NUM_39, TUYA_GPIO_LEVEL_HIGH);
    sg_audio_ready = true;
    return rt;
}

static OPERATE_RET __audio_play_mode(const OUT_AUDIO_MODE_SET_T *set, const OUT_AUDIO_MODE_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    const LOCAL_AUDIO_CLIP_T *clip = NULL;

    if (set == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (set->count == 0) {
        return OPRT_INVALID_PARM;
    }
    rt = __audio_prepare_once();
    if (rt != OPRT_OK) {
        return rt;
    }
    /* Keep player digital volume and output-consumer volume aligned. */
    (void)ai_audio_player_set_vol((int)cfg->volume);
    (void)tuya_ai_player_set_volume(NULL, (int)cfg->volume);
    PR_NOTICE("[AUDIO] apply volume=%u", (unsigned)cfg->volume);

    clip = &set->clips[0];
    rt = ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_START, AI_AUDIO_CODEC_MP3, NULL, 0);
    if (rt != OPRT_OK) {
        return rt;
    }
    while (offset < clip->len) {
        uint32_t remain = clip->len - offset;
        uint32_t send_len = (remain > OUT_AUDIO_CHUNK_SIZE) ? OUT_AUDIO_CHUNK_SIZE : remain;
        rt = ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_DATA, AI_AUDIO_CODEC_MP3,
                                      (char *)(clip->data + offset), send_len);
        if (rt != OPRT_OK) {
            goto __stop;
        }
        offset += send_len;
        tal_system_sleep(20);
    }

__stop:
    (void)ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STOP, AI_AUDIO_CODEC_MP3, NULL, 0);
    return rt;
}

static uint32_t __moto_angle_to_duty(int16_t angle_deg)
{
    int32_t a = angle_deg;
    if (a < 0) {
        a = 0;
    }
    if (a > 180) {
        a = 180;
    }
    return (uint32_t)(OUT_MOTO_PWM_DUTY_MIN +
                      ((a * (int32_t)(OUT_MOTO_PWM_DUTY_MAX - OUT_MOTO_PWM_DUTY_MIN)) / 180));
}

static bool __moto_mode_has_ch0(uint8_t mode)
{
    return (mode == OUTPUT_MODE_1 || mode == OUTPUT_MODE_3);
}

static bool __moto_mode_has_ch1(uint8_t mode)
{
    return (mode == OUTPUT_MODE_2 || mode == OUTPUT_MODE_3);
}

static void __moto_start_mask(uint8_t mode)
{
    if (__moto_mode_has_ch0(mode)) {
        (void)tkl_pwm_start(TUYA_PWM_NUM_2);
    }
    if (__moto_mode_has_ch1(mode)) {
        (void)tkl_pwm_start(TUYA_PWM_NUM_3);
    }
}

static void __moto_stop_mask(uint8_t mode)
{
    if (__moto_mode_has_ch0(mode)) {
        (void)tkl_pwm_stop(TUYA_PWM_NUM_2);
    }
    if (__moto_mode_has_ch1(mode)) {
        (void)tkl_pwm_stop(TUYA_PWM_NUM_3);
    }
}

static void __moto_set_angle_mask(uint8_t mode, int16_t logical_offset_deg)
{
    int16_t target_deg = 0;

    if (__moto_mode_has_ch0(mode)) {
        if (moto_bringup_resolve_target_angle(0, logical_offset_deg, &target_deg) == OPRT_OK) {
            (void)tkl_pwm_duty_set(TUYA_PWM_NUM_2, __moto_angle_to_duty(target_deg));
            sg_moto_logical_offset[0] = logical_offset_deg;
        }
    }
    if (__moto_mode_has_ch1(mode)) {
        if (moto_bringup_resolve_target_angle(1, logical_offset_deg, &target_deg) == OPRT_OK) {
            (void)tkl_pwm_duty_set(TUYA_PWM_NUM_3, __moto_angle_to_duty(target_deg));
            sg_moto_logical_offset[1] = logical_offset_deg;
        }
    }
}

static OPERATE_RET __moto_move_mask(uint8_t mode, int16_t target_offset_deg, uint32_t speed_dps)
{
    int16_t start_ch0 = sg_moto_logical_offset[0];
    int16_t start_ch1 = sg_moto_logical_offset[1];
    int32_t delta_ch0 = 0;
    int32_t delta_ch1 = 0;
    int32_t abs_delta_ch0 = 0;
    int32_t abs_delta_ch1 = 0;
    int32_t max_delta = 0;
    uint32_t duration_ms = 0;
    uint32_t steps = 0;

    if (speed_dps == 0U) {
        return OPRT_INVALID_PARM;
    }

    if (__moto_mode_has_ch0(mode)) {
        delta_ch0 = (int32_t)target_offset_deg - (int32_t)start_ch0;
        abs_delta_ch0 = (delta_ch0 >= 0) ? delta_ch0 : -delta_ch0;
        if (abs_delta_ch0 > max_delta) {
            max_delta = abs_delta_ch0;
        }
    }
    if (__moto_mode_has_ch1(mode)) {
        delta_ch1 = (int32_t)target_offset_deg - (int32_t)start_ch1;
        abs_delta_ch1 = (delta_ch1 >= 0) ? delta_ch1 : -delta_ch1;
        if (abs_delta_ch1 > max_delta) {
            max_delta = abs_delta_ch1;
        }
    }

    if (max_delta <= 0) {
        __moto_set_angle_mask(mode, target_offset_deg);
        return OPRT_OK;
    }

    duration_ms = ((uint32_t)max_delta * 1000U) / speed_dps;
    if (duration_ms < OUT_MOTO_STEP_PERIOD_MS) {
        duration_ms = OUT_MOTO_STEP_PERIOD_MS;
    }

    steps = duration_ms / OUT_MOTO_STEP_PERIOD_MS;
    if (steps == 0U) {
        steps = 1U;
    }

    for (uint32_t i = 1; i <= steps; i++) {
        if (__moto_mode_has_ch0(mode)) {
            int16_t cur_ch0 = (int16_t)(start_ch0 +
                (int32_t)((float)delta_ch0 * ((float)i / (float)steps) +
                          ((delta_ch0 >= 0) ? 0.5f : -0.5f)));
            __moto_set_angle_mask(OUTPUT_MODE_1, cur_ch0);
        }
        if (__moto_mode_has_ch1(mode)) {
            int16_t cur_ch1 = (int16_t)(start_ch1 +
                (int32_t)((float)delta_ch1 * ((float)i / (float)steps) +
                          ((delta_ch1 >= 0) ? 0.5f : -0.5f)));
            __moto_set_angle_mask(OUTPUT_MODE_2, cur_ch1);
        }
        tal_system_sleep(OUT_MOTO_STEP_PERIOD_MS);
    }

    return OPRT_OK;
}

OPERATE_RET output_hal_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_output_inited) {
        return OPRT_OK;
    }

    rt = tal_mutex_create_init(&sg_lcd_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }

    (void)moto_bringup_init();
    sg_moto_logical_offset[0] = 0;
    sg_moto_logical_offset[1] = 0;
    sg_output_inited = true;
    return OPRT_OK;
}

OPERATE_RET output_hal_set_lcd_mode(OUTPUT_MODE_E mode)
{
    OUT_LCD_MODE_SET_T set = {0};
    const OUT_LCD_MODE_CFG_T *cfg = NULL;
    OPERATE_RET rt = OPRT_OK;

    switch (mode) {
    case OUTPUT_MODE_1:
        set.left_frames = g_left_eye_doubt_lcd_frames;
        set.left_count = g_left_eye_doubt_lcd_frame_count;
        set.right_frames = g_right_eye_doubt_lcd_frames;
        set.right_count = g_right_eye_doubt_lcd_frame_count;
        break;
    case OUTPUT_MODE_2:
        set.left_frames = g_left_eye_happy_lcd_frames;
        set.left_count = g_left_eye_happy_lcd_frame_count;
        set.right_frames = g_right_eye_happy_lcd_frames;
        set.right_count = g_right_eye_happy_lcd_frame_count;
        break;
    case OUTPUT_MODE_3:
        set.left_frames = g_left_eye_normal_lcd_frames;
        set.left_count = g_left_eye_normal_lcd_frame_count;
        set.right_frames = g_right_eye_normal_lcd_frames;
        set.right_count = g_right_eye_normal_lcd_frame_count;
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    cfg = &sg_lcd_cfg[(uint8_t)mode - 1U];
    rt = output_hal_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    if (sg_lcd_mutex == NULL) {
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("[LCD] mode start: mode=%u loop=%u fps=%u", (unsigned)mode,
              (unsigned)cfg->loop_time_ms, (unsigned)cfg->fps);

    rt = tal_mutex_lock(sg_lcd_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("[LCD] mutex lock failed: mode=%u rt=%d", (unsigned)mode, rt);
        return rt;
    }

    rt = __lcd_play_mode(&set, cfg);
    (void)tal_mutex_unlock(sg_lcd_mutex);
    return rt;
}

OPERATE_RET output_hal_set_audio_mode(OUTPUT_MODE_E mode)
{
    OUT_AUDIO_MODE_SET_T set = {0};
    const OUT_AUDIO_MODE_CFG_T *cfg = NULL;

    switch (mode) {
    case OUTPUT_MODE_1:
        set.clips = g_doubt_audio_clips;
        set.count = g_doubt_audio_clip_count;
        break;
    case OUTPUT_MODE_2:
        set.clips = g_happy_audio_clips;
        set.count = g_happy_audio_clip_count;
        break;
    case OUTPUT_MODE_3:
        set.clips = g_normal_audio_clips;
        set.count = g_normal_audio_clip_count;
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    cfg = &sg_audio_cfg[(uint8_t)mode - 1U];
    (void)output_hal_init();
    return __audio_play_mode(&set, cfg);
}

OPERATE_RET output_hal_set_moto_mode(OUTPUT_MODE_E mode)
{
    const OUT_MOTO_MODE_CFG_T *cfg = NULL;
    uint64_t start_ms = 0;
    int16_t angle_pos = 0;
    int16_t angle_neg = 0;

    if (mode < OUTPUT_MODE_1 || mode > OUTPUT_MODE_3) {
        return OPRT_INVALID_PARM;
    }

    (void)output_hal_init();
    cfg = &sg_moto_cfg[(uint8_t)mode - 1U];
    if (cfg->angle_deg > 90U || cfg->speed_dps == 0U) {
        return OPRT_INVALID_PARM;
    }
    angle_pos = (int16_t)cfg->angle_deg;
    angle_neg = (int16_t)(-(int16_t)cfg->angle_deg);

    __moto_start_mask((uint8_t)mode);

    if (!cfg->loop) {
        (void)__moto_move_mask((uint8_t)mode, angle_pos, cfg->speed_dps);
        (void)__moto_move_mask((uint8_t)mode, angle_neg, cfg->speed_dps);
    } else {
        start_ms = tal_system_get_millisecond();
        while ((tal_system_get_millisecond() - start_ms) < cfg->run_time_ms) {
            (void)__moto_move_mask((uint8_t)mode, angle_pos, cfg->speed_dps);
            (void)__moto_move_mask((uint8_t)mode, angle_neg, cfg->speed_dps);
        }
    }

    (void)__moto_move_mask((uint8_t)mode, 0, cfg->speed_dps);

    if (!cfg->hold_torque) {
        __moto_stop_mask((uint8_t)mode);
    }
    return OPRT_OK;
}

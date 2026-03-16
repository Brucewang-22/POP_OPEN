/**
 * @file mic_bringup.c
 * @brief Microphone bring-up stats implementation.
 */

#include "mic_bringup.h"

#include "tal_api.h"

typedef struct {
    bool         inited;
    MUTEX_HANDLE lock;

    uint64_t window_start_ms;
    uint32_t frames_window;
    uint32_t rms_sum_window;
    uint32_t rms_cnt_window;
} MIC_BRINGUP_CTX_T;

static MIC_BRINGUP_CTX_T sg_mic_ctx = {0};

static uint32_t __isqrt_u64(uint64_t x)
{
    uint64_t op = x;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;

    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op -= (res + one);
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return (uint32_t)res;
}

static uint32_t __calc_rms_permille(const uint8_t *data, uint32_t data_len)
{
    uint64_t sum_sq = 0;
    uint32_t samples = 0;
    uint32_t i = 0;

    if (data == NULL || data_len < 2) {
        return 0;
    }

    samples = data_len / 2;
    for (i = 0; i < samples; i++) {
        uint16_t raw = (uint16_t)data[2 * i] | ((uint16_t)data[2 * i + 1] << 8);
        int16_t s = (int16_t)raw;
        int32_t v = (int32_t)s;
        sum_sq += (uint64_t)(v * v);
    }

    if (samples == 0) {
        return 0;
    }

    uint32_t mean_sq = (uint32_t)(sum_sq / samples);
    uint32_t rms = __isqrt_u64(mean_sq);
    uint32_t permille = (rms * 1000U) / 32767U;

    if (permille > 1000U) {
        permille = 1000U;
    }

    return permille;
}

static void __mic_roll_window(uint64_t now_ms)
{
    if (sg_mic_ctx.window_start_ms == 0) {
        sg_mic_ctx.window_start_ms = now_ms;
        return;
    }

    if ((now_ms - sg_mic_ctx.window_start_ms) < 1000U) {
        return;
    }

    /* Use rolling 1s buckets; if jump >1s, skip to latest second boundary. */
    uint64_t steps = (now_ms - sg_mic_ctx.window_start_ms) / 1000U;
    sg_mic_ctx.window_start_ms += (steps * 1000U);
    sg_mic_ctx.frames_window = 0;
    sg_mic_ctx.rms_sum_window = 0;
    sg_mic_ctx.rms_cnt_window = 0;
}

OPERATE_RET mic_bringup_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_mic_ctx.inited) {
        return OPRT_OK;
    }

    memset(&sg_mic_ctx, 0, sizeof(sg_mic_ctx));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_mic_ctx.lock));
    sg_mic_ctx.window_start_ms = tal_system_get_millisecond();
    sg_mic_ctx.inited = true;

    return OPRT_OK;
}

void mic_bringup_on_ai_event(AI_NOTIFY_EVENT_T *event)
{
    AI_NOTIFY_MIC_DATA_T *mic_data = NULL;
    uint32_t rms_permille = 0;
    uint64_t now_ms = tal_system_get_millisecond();

    if (!sg_mic_ctx.inited || event == NULL || event->type != AI_USER_EVT_MIC_DATA || event->data == NULL) {
        return;
    }

    mic_data = (AI_NOTIFY_MIC_DATA_T *)event->data;
    rms_permille = __calc_rms_permille(mic_data->data, mic_data->data_len);

    tal_mutex_lock(sg_mic_ctx.lock);
    __mic_roll_window(now_ms);
    sg_mic_ctx.frames_window++;
    sg_mic_ctx.rms_sum_window += rms_permille;
    sg_mic_ctx.rms_cnt_window++;
    tal_mutex_unlock(sg_mic_ctx.lock);
}

OPERATE_RET mic_bringup_get_status(uint32_t *frames_1s, uint32_t *rms_permille)
{
    uint64_t now_ms = tal_system_get_millisecond();

    if (frames_1s == NULL || rms_permille == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_mic_ctx.inited) {
        *frames_1s = 0;
        *rms_permille = 0;
        return OPRT_OK;
    }

    tal_mutex_lock(sg_mic_ctx.lock);
    __mic_roll_window(now_ms);

    *frames_1s = sg_mic_ctx.frames_window;
    *rms_permille = (sg_mic_ctx.rms_cnt_window == 0) ? 0 : (sg_mic_ctx.rms_sum_window / sg_mic_ctx.rms_cnt_window);

    tal_mutex_unlock(sg_mic_ctx.lock);
    return OPRT_OK;
}

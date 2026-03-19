#include "local_audio.h"

#include "tal_api.h"
#include "tkl_gpio.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_player.h"
#include "tdl_audio_manage.h"
#include "test_audio_assets.h"

static THREAD_HANDLE sg_audio_once_thread = NULL;

static void __local_audio_play_once_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    const LOCAL_AUDIO_CLIP_T *clip = NULL;
    const uint32_t chunk_size = 2048;
    uint32_t offset = 0;

    (void)arg;

    if (g_test_audio_clip_count == 0) {
        PR_WARN("[LOCAL][AUDIO] no clip for once-play task");
        return;
    }

    clip = &g_test_audio_clips[0];
    PR_NOTICE("[LOCAL][AUDIO] once-play task start: %s total=%u", clip->name, (unsigned)clip->len);

    TUYA_CALL_ERR_LOG(ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_START, AI_AUDIO_CODEC_MP3, NULL, 0));

    while (offset < clip->len) {
        uint32_t remain = clip->len - offset;
        uint32_t send_len = (remain > chunk_size) ? chunk_size : remain;

        TUYA_CALL_ERR_LOG(ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_DATA, AI_AUDIO_CODEC_MP3,
                                                   (char *)(clip->data + offset), send_len));
        offset += send_len;
        tal_system_sleep(20);
    }

    TUYA_CALL_ERR_LOG(ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STOP, AI_AUDIO_CODEC_MP3, NULL, 0));
    PR_NOTICE("[LOCAL][AUDIO] once-play task finished: sent=%u", (unsigned)offset);
}
#endif

OPERATE_RET local_audio_start_test(void)
{
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T spk_gpio_cfg = {0};
    TDL_AUDIO_HANDLE_T audio_hdl = NULL;
    THREAD_CFG_T cfg = {0};

    if (g_test_audio_clip_count == 0) {
        PR_WARN("[LOCAL][AUDIO] no mp3 clip asset found");
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(audio_hdl, NULL));
    PR_NOTICE("[LOCAL][AUDIO] tdl_audio_open(%s) ok", AUDIO_CODEC_NAME);

    TUYA_CALL_ERR_RETURN(ai_audio_player_init());
    TUYA_CALL_ERR_LOG(ai_audio_player_set_vol(70));

    spk_gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    spk_gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    spk_gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(TUYA_GPIO_NUM_39, &spk_gpio_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_write(TUYA_GPIO_NUM_39, TUYA_GPIO_LEVEL_HIGH));
    PR_NOTICE("[LOCAL][AUDIO] force PA enable GPIO P39=HIGH");

    cfg.stackDepth = 3072;
    cfg.priority = THREAD_PRIO_2;
    cfg.thrdname = "audio_once";
    TUYA_CALL_ERR_RETURN(
        tal_thread_create_and_start(&sg_audio_once_thread, NULL, NULL, __local_audio_play_once_task, NULL, &cfg));

    PR_NOTICE("[LOCAL][AUDIO] play clip: %s len=%u", g_test_audio_clips[0].name, (unsigned)g_test_audio_clips[0].len);
    return rt;
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

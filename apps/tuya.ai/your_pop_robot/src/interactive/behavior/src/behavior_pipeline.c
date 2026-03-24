/**
 * @file behavior_pipeline.c
 * @brief Interactive behavior pipeline implementation.
 */

#include "behavior_pipeline.h"

#include <stdint.h>
#include "tal_api.h"
#include "output_hal.h"
#include "hardware_abstraction.h"
#include "input_trigger_layer.h"

#define BEHAVIOR_TOUCH_CH_CNT                3
#define BEHAVIOR_INTERACT_COOLDOWN_MS        2000U
#define BEHAVIOR_OUTPUT_THREAD_STACK_DEPTH   4096U
#define BEHAVIOR_OUTPUT_THREAD_PRIORITY      THREAD_PRIO_2

typedef enum {
    BEHAVIOR_STATE_IDLE = 0,
    BEHAVIOR_STATE_RUNNING,
    BEHAVIOR_STATE_COOLDOWN,
} BEHAVIOR_STATE_E;

typedef struct {
    bool touch_triggered;
    bool touch_rising;
    uint8_t touch_rising_mask;
    bool imu_shake;
    bool mic_triggered;
    bool imu_rising;
} BEHAVIOR_INPUT_SNAPSHOT_T;

typedef struct {
    uint8_t id;
    uint8_t in_touch1_en;
    uint8_t in_touch2_en;
    uint8_t in_touch3_en;
    uint8_t in_mic_en;
    uint8_t in_imu_en;
    uint8_t out_lcd_en;
    uint8_t out_audio_en;
    uint8_t out_moto_en;
    OUTPUT_MODE_E lcd_mode;
    OUTPUT_MODE_E audio_mode;
    OUTPUT_MODE_E moto_mode;
} BEHAVIOR_RULE_CFG_T;

typedef struct {
    bool inited;
    BEHAVIOR_STATE_E state;
    uint8_t active_rule_id;
    uint64_t cooldown_until_ms;
    bool prev_touch_triggered;
    bool prev_imu_shake;
    const INTERACTIVE_HARDWARE_ABSTRACTION_T *hw;
} BEHAVIOR_CONTEXT_T;

static BEHAVIOR_CONTEXT_T sg_behavior_ctx = {
    .inited = false,
    .state = BEHAVIOR_STATE_IDLE,
    .active_rule_id = 0,
    .cooldown_until_ms = 0,
    .prev_touch_triggered = false,
    .prev_imu_shake = false,
    .hw = NULL,
};

/*
 * Interaction rules:
 * - 3 inputs are always collected: touch / microphone / imu.
 * - Each input/output uses 0/1 switches only.
 * - Input side: enabled source can trigger this rule.
 * - Output side: enabled actuator is applied with its mode.
 */
static const BEHAVIOR_RULE_CFG_T sg_behavior_rules[] = {
    /* rule1: touch1 -> lcd1 + audio1 + moto1 */
    {.id = 1, .in_touch1_en = 1, .in_touch2_en = 0, .in_touch3_en = 0, .in_mic_en = 0, .in_imu_en = 0,
     .out_lcd_en = 1, .out_audio_en = 1, .out_moto_en = 1,
     .lcd_mode = OUTPUT_MODE_1, .audio_mode = OUTPUT_MODE_1, .moto_mode = OUTPUT_MODE_1},
    /* rule2: touch2 -> lcd2 + audio2 + moto2 */
    {.id = 2, .in_touch1_en = 0, .in_touch2_en = 1, .in_touch3_en = 0, .in_mic_en = 0, .in_imu_en = 0,
     .out_lcd_en = 1, .out_audio_en = 1, .out_moto_en = 1,
     .lcd_mode = OUTPUT_MODE_2, .audio_mode = OUTPUT_MODE_2, .moto_mode = OUTPUT_MODE_2},
    /* rule3: touch3 -> lcd3 + audio3 + moto3 */
    {.id = 3, .in_touch1_en = 0, .in_touch2_en = 0, .in_touch3_en = 1, .in_mic_en = 0, .in_imu_en = 0,
     .out_lcd_en = 1, .out_audio_en = 1, .out_moto_en = 1,
     .lcd_mode = OUTPUT_MODE_3, .audio_mode = OUTPUT_MODE_3, .moto_mode = OUTPUT_MODE_3},
    /* rule4: imu shake -> lcd1 + audio1 + moto1 */
    {.id = 4, .in_touch1_en = 0, .in_touch2_en = 0, .in_touch3_en = 0, .in_mic_en = 0, .in_imu_en = 1,
     .out_lcd_en = 1, .out_audio_en = 1, .out_moto_en = 1,
     .lcd_mode = OUTPUT_MODE_1, .audio_mode = OUTPUT_MODE_1, .moto_mode = OUTPUT_MODE_1},
};

static void __behavior_async_lcd_task(void *arg)
{
    OUTPUT_MODE_E mode = (OUTPUT_MODE_E)(uintptr_t)arg;
    OPERATE_RET rt = output_hal_set_lcd_mode(mode);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][ASYNC] lcd mode=%u failed: %d", (unsigned)mode, rt);
    }
}

static void __behavior_async_audio_task(void *arg)
{
    OUTPUT_MODE_E mode = (OUTPUT_MODE_E)(uintptr_t)arg;
    OPERATE_RET rt = output_hal_set_audio_mode(mode);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][ASYNC] audio mode=%u failed: %d", (unsigned)mode, rt);
    }
}

static void __behavior_async_moto_task(void *arg)
{
    OUTPUT_MODE_E mode = (OUTPUT_MODE_E)(uintptr_t)arg;
    OPERATE_RET rt = output_hal_set_moto_mode(mode);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][ASYNC] moto mode=%u failed: %d", (unsigned)mode, rt);
    }
}

static OPERATE_RET __behavior_spawn_output_task(char *name, THREAD_FUNC_CB func, OUTPUT_MODE_E mode)
{
    THREAD_HANDLE th = NULL;
    THREAD_CFG_T cfg = {0};

    cfg.stackDepth = BEHAVIOR_OUTPUT_THREAD_STACK_DEPTH;
    cfg.priority = BEHAVIOR_OUTPUT_THREAD_PRIORITY;
    cfg.thrdname = name;
    return tal_thread_create_and_start(&th, NULL, NULL, func, (void *)(uintptr_t)mode, &cfg);
}

static void __behavior_collect_inputs(BEHAVIOR_INPUT_SNAPSHOT_T *snapshot)
{
    INPUT_TRIGGER_FLAGS_T flags = {0};
    INPUT_TRIGGER_TOUCH_DETAIL_T touch_detail = {0};

    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));

    if (sg_behavior_ctx.hw == NULL) {
        return;
    }

    if (input_trigger_layer_get_flags(&flags) == OPRT_OK) {
        snapshot->touch_triggered = (flags.touch != 0U);
        snapshot->mic_triggered = (flags.microphone != 0U);
        snapshot->imu_shake = (flags.imu != 0U);
    }
    if (input_trigger_layer_get_touch_detail(&touch_detail) == OPRT_OK) {
        snapshot->touch_rising = (touch_detail.touch_threshold_rising != 0U);
        snapshot->touch_rising_mask = touch_detail.touch_rising_mask;
    } else {
        snapshot->touch_rising = (snapshot->touch_triggered && !sg_behavior_ctx.prev_touch_triggered);
        snapshot->touch_rising_mask = 0U;
    }
    sg_behavior_ctx.prev_touch_triggered = snapshot->touch_triggered;
    snapshot->imu_rising = (snapshot->imu_shake && !sg_behavior_ctx.prev_imu_shake);
    sg_behavior_ctx.prev_imu_shake = snapshot->imu_shake;
}

static bool __behavior_rule_match(const BEHAVIOR_RULE_CFG_T *rule, const BEHAVIOR_INPUT_SNAPSHOT_T *in)
{
    bool touch1_hit = false;
    bool touch2_hit = false;
    bool touch3_hit = false;
    bool mic_hit = false;
    bool imu_hit = false;
    bool any_input_enabled = false;

    if (rule == NULL || in == NULL) {
        return false;
    }

    /*
     * Touch trigger is gated by aggregated threshold rising, while channel
     * comes from per-channel rising mask.
     */
    touch1_hit = (in->touch_rising && ((in->touch_rising_mask & (uint8_t)(1U << 0)) != 0U));
    touch2_hit = (in->touch_rising && ((in->touch_rising_mask & (uint8_t)(1U << 1)) != 0U));
    touch3_hit = (in->touch_rising && ((in->touch_rising_mask & (uint8_t)(1U << 2)) != 0U));
    mic_hit = in->mic_triggered;
    imu_hit = in->imu_rising;

    if (rule->in_touch1_en > 0U) {
        any_input_enabled = true;
        if (touch1_hit) {
            return true;
        }
    }
    if (rule->in_touch2_en > 0U) {
        any_input_enabled = true;
        if (touch2_hit) {
            return true;
        }
    }
    if (rule->in_touch3_en > 0U) {
        any_input_enabled = true;
        if (touch3_hit) {
            return true;
        }
    }
    if (rule->in_mic_en > 0U) {
        any_input_enabled = true;
        if (mic_hit) {
            return true;
        }
    }
    if (rule->in_imu_en > 0U) {
        any_input_enabled = true;
        if (imu_hit) {
            return true;
        }
    }

    if (!any_input_enabled) {
        return false;
    }
    return false;
}

static const BEHAVIOR_RULE_CFG_T *__behavior_pick_rule(const BEHAVIOR_INPUT_SNAPSHOT_T *in)
{
    uint32_t i = 0;
    for (i = 0; i < (sizeof(sg_behavior_rules) / sizeof(sg_behavior_rules[0])); i++) {
        if (__behavior_rule_match(&sg_behavior_rules[i], in)) {
            return &sg_behavior_rules[i];
        }
    }
    return NULL;
}

static OPERATE_RET __behavior_run_rule(const BEHAVIOR_RULE_CFG_T *rule)
{
    OPERATE_RET first_err = OPRT_OK;
    OPERATE_RET rt = OPRT_OK;

    if (rule == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (rule->out_lcd_en > 0U) {
        rt = __behavior_spawn_output_task("bh_lcd", __behavior_async_lcd_task, rule->lcd_mode);
        if (rt != OPRT_OK && first_err == OPRT_OK) {
            first_err = rt;
        }
    }
    if (rule->out_audio_en > 0U) {
        rt = __behavior_spawn_output_task("bh_aud", __behavior_async_audio_task, rule->audio_mode);
        if (rt != OPRT_OK && first_err == OPRT_OK) {
            first_err = rt;
        }
    }
    if (rule->out_moto_en > 0U) {
        rt = __behavior_spawn_output_task("bh_moto", __behavior_async_moto_task, rule->moto_mode);
        if (rt != OPRT_OK && first_err == OPRT_OK) {
            first_err = rt;
        }
    }
    return first_err;
}

static void __behavior_interaction_init_once(void)
{
    if (sg_behavior_ctx.inited) {
        return;
    }
    sg_behavior_ctx.hw = interactive_get_hardware_abstraction();
    sg_behavior_ctx.inited = true;
}

OPERATE_RET behavior_pipeline_init_phase(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = output_hal_set_lcd_mode(OUTPUT_MODE_1);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][INIT] lcd model1 failed: %d", rt);
        return rt;
    }

    rt = output_hal_set_audio_mode(OUTPUT_MODE_1);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][INIT] audio model1 failed: %d", rt);
        return rt;
    }

    rt = output_hal_set_moto_mode(OUTPUT_MODE_1);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][INIT] moto model1 failed: %d", rt);
        return rt;
    }

    PR_NOTICE("[BEHAVIOR][INIT] combo done: lcd1 + audio1 + moto1");
    return OPRT_OK;
}

OPERATE_RET behavior_pipeline_interaction_phase(void)
{
    BEHAVIOR_INPUT_SNAPSHOT_T input = {0};
    const BEHAVIOR_RULE_CFG_T *picked = NULL;
    OPERATE_RET rt = OPRT_OK;
    uint64_t now_ms = tal_system_get_millisecond();

    __behavior_interaction_init_once();
    if (sg_behavior_ctx.hw == NULL) {
        return OPRT_COM_ERROR;
    }

    __behavior_collect_inputs(&input);

    if (sg_behavior_ctx.state == BEHAVIOR_STATE_COOLDOWN) {
        if (now_ms < sg_behavior_ctx.cooldown_until_ms) {
            return OPRT_OK;
        }
        sg_behavior_ctx.state = BEHAVIOR_STATE_IDLE;
        sg_behavior_ctx.active_rule_id = 0;
    }

    if (sg_behavior_ctx.state != BEHAVIOR_STATE_IDLE) {
        return OPRT_OK;
    }

    picked = __behavior_pick_rule(&input);
    if (picked == NULL) {
        if (input.touch_rising || input.imu_rising) {
            PR_NOTICE("[BEHAVIOR][INTERACT] no-rule: touch=%u rising=%u rising_mask=0x%02X mic=%u imu=%u imu_rising=%u",
                      (unsigned)input.touch_triggered, (unsigned)input.touch_rising,
                      (unsigned)input.touch_rising_mask,
                      (unsigned)input.mic_triggered, (unsigned)input.imu_shake,
                      (unsigned)input.imu_rising);
        }
        return OPRT_OK;
    }

    sg_behavior_ctx.state = BEHAVIOR_STATE_RUNNING;
    sg_behavior_ctx.active_rule_id = picked->id;

    rt = __behavior_run_rule(picked);
    if (rt != OPRT_OK) {
        PR_WARN("[BEHAVIOR][INTERACT] rule=%u run failed: %d", (unsigned)picked->id, rt);
    } else {
        PR_NOTICE("[BEHAVIOR][INTERACT] rule=%u run ok => lcd(%u,%u) audio(%u,%u) moto(%u,%u)",
                  (unsigned)picked->id, (unsigned)picked->lcd_mode,
                  (unsigned)picked->out_lcd_en,
                  (unsigned)picked->audio_mode, (unsigned)picked->out_audio_en,
                  (unsigned)picked->moto_mode, (unsigned)picked->out_moto_en);
    }

    sg_behavior_ctx.state = BEHAVIOR_STATE_COOLDOWN;
    sg_behavior_ctx.cooldown_until_ms = tal_system_get_millisecond() + BEHAVIOR_INTERACT_COOLDOWN_MS;
    return OPRT_OK;
}

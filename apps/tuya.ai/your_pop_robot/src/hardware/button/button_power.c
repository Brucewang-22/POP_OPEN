/**
 * @file button_power.c
 * @brief Long-press power switch logic using P36 output and P37 button input.
 */

#include "button_power.h"

#include <string.h>

#include "tal_log.h"
#include "tkl_gpio.h"

#define BUTTON_POWER_SWITCH_PIN            TUYA_GPIO_NUM_36
#define BUTTON_POWER_BUTTON_PIN            TUYA_GPIO_NUM_37
#define BUTTON_POWER_SWITCH_ON_LEVEL       TUYA_GPIO_LEVEL_HIGH
#define BUTTON_POWER_SWITCH_OFF_LEVEL      TUYA_GPIO_LEVEL_LOW
#define BUTTON_POWER_BUTTON_ACTIVE_LEVEL   TUYA_GPIO_LEVEL_LOW
#define BUTTON_POWER_BUTTON_IDLE_LEVEL     TUYA_GPIO_LEVEL_HIGH
#define BUTTON_POWER_ON_TRIGGER_MS         1500U
#define BUTTON_POWER_OFF_TRIGGER_MS        500U
typedef struct {
    bool inited;
    bool power_enabled;
    bool hold_latched;
    bool button_pressed;
    bool long_press_armed;
    uint32_t press_duration_ms;
} BUTTON_POWER_CTX_T;

static BUTTON_POWER_CTX_T sg_button_power;

static OPERATE_RET __button_power_set_output(bool enabled)
{
    TUYA_GPIO_LEVEL_E level = enabled ? BUTTON_POWER_SWITCH_ON_LEVEL : BUTTON_POWER_SWITCH_OFF_LEVEL;
    OPERATE_RET rt = tkl_gpio_write(BUTTON_POWER_SWITCH_PIN, level);

    if (rt != OPRT_OK) {
        return rt;
    }

    sg_button_power.power_enabled = enabled;
    PR_NOTICE("[BUTTON] switch pin=%d level=%d power=%u", (int)BUTTON_POWER_SWITCH_PIN, (int)level,
              (unsigned)(enabled ? 1U : 0U));
    return OPRT_OK;
}

static OPERATE_RET __button_power_set_hold_latch(bool enabled)
{
    TUYA_GPIO_LEVEL_E level = enabled ? BUTTON_POWER_SWITCH_ON_LEVEL : BUTTON_POWER_SWITCH_OFF_LEVEL;
    OPERATE_RET rt = tkl_gpio_write(BUTTON_POWER_SWITCH_PIN, level);

    if (rt != OPRT_OK) {
        return rt;
    }

    sg_button_power.hold_latched = enabled;
    PR_NOTICE("[BUTTON] hold latch pin=%d level=%d latched=%u", (int)BUTTON_POWER_SWITCH_PIN, (int)level,
              (unsigned)(enabled ? 1U : 0U));
    return OPRT_OK;
}

static bool __button_power_is_pressed(TUYA_GPIO_LEVEL_E level)
{
    return (level == BUTTON_POWER_BUTTON_ACTIVE_LEVEL);
}

static uint32_t __button_power_get_trigger_ms(void)
{
    return sg_button_power.power_enabled ? BUTTON_POWER_OFF_TRIGGER_MS : BUTTON_POWER_ON_TRIGGER_MS;
}

OPERATE_RET button_power_init(void)
{
    TUYA_GPIO_BASE_CFG_T switch_cfg = {0};
    TUYA_GPIO_BASE_CFG_T button_cfg = {0};
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_button_power, 0, sizeof(sg_button_power));

    switch_cfg.direct = TUYA_GPIO_OUTPUT;
    switch_cfg.mode = TUYA_GPIO_PUSH_PULL;
    switch_cfg.level = BUTTON_POWER_SWITCH_ON_LEVEL;
    rt = tkl_gpio_init(BUTTON_POWER_SWITCH_PIN, &switch_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("[BUTTON] switch init failed: pin=%d rt=%d", (int)BUTTON_POWER_SWITCH_PIN, rt);
        return rt;
    }

    button_cfg.direct = TUYA_GPIO_INPUT;
    button_cfg.mode = TUYA_GPIO_PULLUP;
    button_cfg.level = BUTTON_POWER_BUTTON_IDLE_LEVEL;
    rt = tkl_gpio_init(BUTTON_POWER_BUTTON_PIN, &button_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("[BUTTON] button init failed: pin=%d rt=%d", (int)BUTTON_POWER_BUTTON_PIN, rt);
        return rt;
    }

    sg_button_power.inited = true;
    sg_button_power.hold_latched = true;
    sg_button_power.long_press_armed = true;

    PR_NOTICE("[BUTTON] init ok: switch=P36 button=P37 on_ms=%u off_ms=%u switch_on=%d button_active=%d boot_hold=%u",
              (unsigned)BUTTON_POWER_ON_TRIGGER_MS, (unsigned)BUTTON_POWER_OFF_TRIGGER_MS,
              (int)BUTTON_POWER_SWITCH_ON_LEVEL,
              (int)BUTTON_POWER_BUTTON_ACTIVE_LEVEL,
              (unsigned)(sg_button_power.hold_latched ? 1U : 0U));
    return OPRT_OK;
}

OPERATE_RET button_power_poll(uint32_t elapsed_ms)
{
    TUYA_GPIO_LEVEL_E level = BUTTON_POWER_BUTTON_IDLE_LEVEL;
    bool pressed = false;
    OPERATE_RET rt = OPRT_OK;
    uint32_t trigger_ms = 0;

    if (!sg_button_power.inited) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_gpio_read(BUTTON_POWER_BUTTON_PIN, &level);
    if (rt != OPRT_OK) {
        return rt;
    }

    pressed = __button_power_is_pressed(level);
    sg_button_power.button_pressed = pressed;
    trigger_ms = __button_power_get_trigger_ms();

    if (!pressed) {
        if (!sg_button_power.power_enabled && sg_button_power.hold_latched) {
            rt = __button_power_set_hold_latch(false);
            if (rt != OPRT_OK) {
                return rt;
            }
        }
        sg_button_power.press_duration_ms = 0;
        sg_button_power.long_press_armed = true;
        return OPRT_OK;
    }

    if (!sg_button_power.power_enabled && !sg_button_power.hold_latched) {
        rt = __button_power_set_hold_latch(true);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    if (sg_button_power.press_duration_ms < UINT32_MAX - elapsed_ms) {
        sg_button_power.press_duration_ms += elapsed_ms;
    } else {
        sg_button_power.press_duration_ms = UINT32_MAX;
    }

    if (sg_button_power.long_press_armed &&
        sg_button_power.press_duration_ms >= trigger_ms) {
        sg_button_power.long_press_armed = false;
        if (!sg_button_power.power_enabled) {
            rt = __button_power_set_output(true);
            if (rt != OPRT_OK) {
                return rt;
            }
            sg_button_power.hold_latched = true;
        } else {
            rt = __button_power_set_output(false);
            if (rt != OPRT_OK) {
                return rt;
            }
            sg_button_power.hold_latched = false;
        }
        PR_NOTICE("[BUTTON] long press toggle: pressed_ms=%u trigger_ms=%u power=%u",
                  (unsigned)sg_button_power.press_duration_ms,
                  (unsigned)trigger_ms,
                  (unsigned)(sg_button_power.power_enabled ? 1U : 0U));
    }

    return OPRT_OK;
}

OPERATE_RET button_power_get_status(BUTTON_POWER_STATUS_T *status)
{
    if (status == NULL || !sg_button_power.inited) {
        return OPRT_INVALID_PARM;
    }

    status->power_enabled = sg_button_power.power_enabled;
    status->button_pressed = sg_button_power.button_pressed;
    status->long_press_armed = sg_button_power.long_press_armed;
    status->press_duration_ms = sg_button_power.press_duration_ms;
    return OPRT_OK;
}

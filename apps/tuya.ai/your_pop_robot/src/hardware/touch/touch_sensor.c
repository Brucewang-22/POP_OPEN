/**
 * @file touch_sensor.c
 * @brief External touch switch sensor (VCC/GND/GPIO) implementation, multi-channel.
 */

#include <string.h>

#include "touch_sensor.h"
#include "tal_log.h"
#include "tkl_gpio.h"

static bool sg_inited[TOUCH_SENSOR_CH_MAX];
static TOUCH_SENSOR_CFG_T sg_cfg[TOUCH_SENSOR_CH_MAX];

OPERATE_RET touch_sensor_init_channel(uint8_t ch, const TOUCH_SENSOR_CFG_T *cfg)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {0};

    if (cfg == NULL || ch >= TOUCH_SENSOR_CH_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (cfg->gpio_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    memset(&sg_cfg[ch], 0, sizeof(sg_cfg[ch]));
    sg_cfg[ch] = *cfg;

    gpio_cfg.direct = TUYA_GPIO_INPUT;
    gpio_cfg.mode   = sg_cfg[ch].pull;
    gpio_cfg.level  = (sg_cfg[ch].active_level == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;

    OPERATE_RET rt = tkl_gpio_init(sg_cfg[ch].gpio_pin, &gpio_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("touch_sensor_init_channel: ch=%u pin=%d rt=%d", ch, sg_cfg[ch].gpio_pin, rt);
        return rt;
    }

    sg_inited[ch] = true;
    PR_NOTICE("touch_sensor_init_channel: ch=%u pin=%d active=%d pull=%d", ch, sg_cfg[ch].gpio_pin, sg_cfg[ch].active_level,
              sg_cfg[ch].pull);
    return OPRT_OK;
}

OPERATE_RET touch_sensor_deinit(void)
{
    for (uint8_t ch = 0; ch < TOUCH_SENSOR_CH_MAX; ch++) {
        if (!sg_inited[ch]) {
            continue;
        }
        OPERATE_RET rt = tkl_gpio_deinit(sg_cfg[ch].gpio_pin);
        if (rt != OPRT_OK) {
            PR_WARN("touch_sensor_deinit: ch=%u pin=%d rt=%d", ch, sg_cfg[ch].gpio_pin, rt);
        }
        sg_inited[ch] = false;
        memset(&sg_cfg[ch], 0, sizeof(sg_cfg[ch]));
    }
    return OPRT_OK;
}

OPERATE_RET touch_sensor_read_channel(uint8_t ch, bool *touched)
{
    TUYA_GPIO_LEVEL_E lv = TUYA_GPIO_LEVEL_LOW;

    if (touched == NULL || ch >= TOUCH_SENSOR_CH_MAX) {
        return OPRT_INVALID_PARM;
    }
    if (!sg_inited[ch]) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = tkl_gpio_read(sg_cfg[ch].gpio_pin, &lv);
    if (rt != OPRT_OK) {
        return rt;
    }

    *touched = (lv == sg_cfg[ch].active_level);
    return OPRT_OK;
}

TUYA_GPIO_NUM_E touch_sensor_get_pin_channel(uint8_t ch)
{
    if (ch >= TOUCH_SENSOR_CH_MAX || !sg_inited[ch]) {
        return TUYA_GPIO_NUM_MAX;
    }
    return sg_cfg[ch].gpio_pin;
}

OPERATE_RET touch_sensor_init(const TOUCH_SENSOR_CFG_T *cfg)
{
    return touch_sensor_init_channel(0, cfg);
}

OPERATE_RET touch_sensor_read(bool *touched)
{
    return touch_sensor_read_channel(0, touched);
}

TUYA_GPIO_NUM_E touch_sensor_get_pin(void)
{
    return touch_sensor_get_pin_channel(0);
}


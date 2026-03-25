/**
 * @file hardware_abstraction.c
 * @brief Interactive hardware abstraction implementation.
 */

#include "hardware_abstraction.h"

#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
#include "mic_bringup.h"
#endif

#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
#include "touch_sensor.h"
#endif

#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
#include "imu_ahrs.h"
#endif

#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
#include "lcd_gc9d01.h"
#endif

#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
#include "moto_bringup.h"
#endif

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "local_audio.h"
#endif

#if defined(__GNUC__)
#define HA_UNUSED __attribute__((unused))
#else
#define HA_UNUSED
#endif

static OPERATE_RET HA_UNUSED __ha_not_supported_void(void)
{
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_mic_status(uint32_t *frames_1s, uint32_t *rms_permille)
{
    (void)frames_1s;
    (void)rms_permille;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_touch_read(uint8_t ch, bool *touched)
{
    (void)ch;
    (void)touched;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_touch_init_channel(
    uint8_t ch, const INTERACTIVE_TOUCH_CHANNEL_CFG_T *cfg)
{
    (void)ch;
    (void)cfg;
    return OPRT_NOT_SUPPORTED;
}

static TUYA_GPIO_NUM_E HA_UNUSED __ha_not_supported_touch_get_pin_channel(uint8_t ch)
{
    (void)ch;
    return TUYA_GPIO_NUM_MAX;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_imu_status(IMU_AHRS_STATUS_T *status)
{
    (void)status;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_lcd_clear(LCD_INSTANCE_E instance)
{
    (void)instance;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET HA_UNUSED __ha_not_supported_lcd_fill_color(LCD_INSTANCE_E instance, uint16_t color_rgb565)
{
    (void)instance;
    (void)color_rgb565;
    return OPRT_NOT_SUPPORTED;
}

#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
static OPERATE_RET __ha_touch_init_channel(uint8_t ch, const INTERACTIVE_TOUCH_CHANNEL_CFG_T *cfg)
{
    TOUCH_SENSOR_CFG_T touch_cfg;

    if (cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    touch_cfg.gpio_pin = cfg->gpio_pin;
    touch_cfg.active_level = cfg->active_level;
    touch_cfg.pull = cfg->pull;
    return touch_sensor_init_channel(ch, &touch_cfg);
}
#endif

static const INTERACTIVE_HARDWARE_ABSTRACTION_T sg_ha = {
    .input = {
#if defined(ENABLE_HARDWARE_MICROPHONE) && (ENABLE_HARDWARE_MICROPHONE == 1)
        .microphone_init = mic_bringup_init,
        .microphone_get_status = mic_bringup_get_status,
#else
        .microphone_init = __ha_not_supported_void,
        .microphone_get_status = __ha_not_supported_mic_status,
#endif
#if defined(ENABLE_HARDWARE_TOUCH) && (ENABLE_HARDWARE_TOUCH == 1)
        .touch_init_channel = __ha_touch_init_channel,
        .touch_read_channel = touch_sensor_read_channel,
        .touch_get_pin_channel = touch_sensor_get_pin_channel,
#else
        .touch_init_channel = __ha_not_supported_touch_init_channel,
        .touch_read_channel = __ha_not_supported_touch_read,
        .touch_get_pin_channel = __ha_not_supported_touch_get_pin_channel,
#endif
#if defined(ENABLE_HARDWARE_IMU) && (ENABLE_HARDWARE_IMU == 1)
        .imu_init = imu_ahrs_init,
        .imu_poll = imu_ahrs_poll,
        .imu_get_status = imu_ahrs_get_status,
#else
        .imu_init = __ha_not_supported_void,
        .imu_poll = __ha_not_supported_void,
        .imu_get_status = __ha_not_supported_imu_status,
#endif
    },
    .output = {
#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD == 1)
        .lcd_clear = lcd_gc9d01_clear,
        .lcd_fill_color = lcd_gc9d01_fill_color,
#else
        .lcd_clear = __ha_not_supported_lcd_clear,
        .lcd_fill_color = __ha_not_supported_lcd_fill_color,
#endif
#if defined(ENABLE_HARDWARE_MOTO) && (ENABLE_HARDWARE_MOTO == 1)
        .moto_init = moto_bringup_init,
#else
        .moto_init = __ha_not_supported_void,
#endif
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        .audio_start_test = local_audio_start_test,
#else
        .audio_start_test = __ha_not_supported_void,
#endif
    },
};

const INTERACTIVE_HARDWARE_ABSTRACTION_T *interactive_get_hardware_abstraction(void)
{
    return &sg_ha;
}

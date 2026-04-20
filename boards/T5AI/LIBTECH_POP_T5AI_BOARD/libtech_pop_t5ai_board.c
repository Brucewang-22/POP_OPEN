/**
 * @file libtech_pop_t5ai_board.c
 * @brief LIBTECH_POP_T5AI_BOARD board support. Compatible with TUYA_T5AI_CORE (audio/button/LED)
 *        and T5AI_OTTO expansion modules (ST7789, ST7735S_XLT, GC9D01).
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_gpio.h"

#include "tdd_audio.h"
#if defined(LED_NAME)
#include "tdd_led_gpio.h"
#endif
#if defined(BUTTON_NAME)
#include "tdd_button_gpio.h"
#endif

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1)
#include "tdd_disp_st7789.h"
#elif defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1)
#include "tdd_disp_st7735s.h"
#elif defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
#include "tdd_disp_gc9d01.h"
#endif

#define BOARD_DISPLAY_NAME_0 "display0"
#define BOARD_DISPLAY_NAME_1 "display1"

/***********************************************************
************************macro define************************
***********************************************************/
/* Base hardware: compatible with TUYA_T5AI_CORE pinout */
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_39

#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_29
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN       TUYA_GPIO_NUM_9
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

/* CONNECTION.md: dual GC9D01 backlight control pins */
#define BOARD_GC9D01_LCD0_BL_PIN TUYA_GPIO_NUM_18
#define BOARD_GC9D01_LCD1_BL_PIN TUYA_GPIO_NUM_6

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1)
#define BOARD_LCD_BL_TYPE      TUYA_DISP_BL_TP_GPIO
#define BOARD_LCD_BL_PIN       TUYA_GPIO_NUM_5
#define BOARD_LCD_BL_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH      240
#define BOARD_LCD_HEIGHT     240
#define BOARD_LCD_X_OFFSET   0
#define BOARD_LCD_Y_OFFSET   0

#elif defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1)
#define BOARD_LCD_BL_TYPE      TUYA_DISP_BL_TP_GPIO
#define BOARD_LCD_BL_PIN       TUYA_GPIO_NUM_5
#define BOARD_LCD_BL_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH      160
#define BOARD_LCD_HEIGHT     80
#define BOARD_LCD_X_OFFSET   1
#define BOARD_LCD_Y_OFFSET   0x1A

#elif defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
/* CONNECTION.md: LCD0 LEDK -> P18, LCD1 LEDK -> P6, both are active-low */
#define BOARD_LCD_BL_TYPE      TUYA_DISP_BL_TP_GPIO
#define BOARD_LCD_BL_PIN       BOARD_GC9D01_LCD0_BL_PIN
#define BOARD_LCD_BL_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_WIDTH      160
#define BOARD_LCD_HEIGHT     160
#define BOARD_LCD_X_OFFSET   0
#define BOARD_LCD_Y_OFFSET   0
#define BOARD_LCD_SPI_CS_PIN  TUYA_GPIO_NUM_15

#define BOARD_LCD_SPI2_PORT    TUYA_SPI_NUM_1
#define BOARD_LCD_SPI2_CLK     48000000
#define BOARD_LCD_SPI2_CS_PIN  TUYA_GPIO_NUM_3
#define BOARD_LCD_SPI2_DC_PIN  TUYA_GPIO_NUM_5
#define BOARD_LCD_SPI2_RST_PIN TUYA_GPIO_NUM_7

#define BOARD_LCD2_BL_TYPE      TUYA_DISP_BL_TP_GPIO
#define BOARD_LCD2_BL_PIN       BOARD_GC9D01_LCD1_BL_PIN
#define BOARD_LCD2_BL_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#endif

#if !defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) || (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 != 1)
#define BOARD_LCD_SPI_CS_PIN  TUYA_GPIO_NUM_13
#endif
#define BOARD_LCD_PIXELS_FMT  TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION    TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_SPI_PORT    TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK     48000000

#define BOARD_LCD_SPI_DC_PIN  TUYA_GPIO_NUM_17
#define BOARD_LCD_SPI_RST_PIN TUYA_GPIO_NUM_19

#define BOARD_LCD_POWER_PIN       TUYA_GPIO_NUM_MAX
#define BOARD_LCD_POWER_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1)
static const uint8_t cST7735S_XLT_INIT_SEQ[] = {
    1,    120,  0x11,
    1,    0,  0x21,
    1,    0,  0x21,
    4,    100,  0xB1, 0x05, 0x3A, 0x3A,
    4,    0,    0xB2, 0x05, 0x3A, 0x3A,
    7,    0,    0xB3, 0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A,
    2,    0,    0xB4, 0x03,
    4,    0,    0xC0, 0x62, 0x02, 0x04,
    2,    0,    0xC1, 0xC0,
    3,    0,    0xC2, 0x0D, 0x00,
    3,    0,    0xC3, 0x8A, 0x6A,
    3,    0,    0xC4, 0x8D, 0xEE,
    2,    0,    0xC5, 0x0E,
    17,   0,    0xE0, 0x10, 0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D, 0x0E, 0x10,
    17,   0,    0xE1, 0x10, 0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D, 0x0E, 0x10,
    2,    0,    0x3A, 0x05,
    2,    0,    0x36, 0xA8,
    1,    0,    0x29,
    0
};
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

#if defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1) \
    || defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD != 1)
static VOID_T __board_gc9d01_backlight_force_off_if_disabled(VOID_T)
{
#if defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD != 1)
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };

    tkl_gpio_init(BOARD_GC9D01_LCD0_BL_PIN, &gpio_cfg);
    tkl_gpio_write(BOARD_GC9D01_LCD0_BL_PIN, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_init(BOARD_GC9D01_LCD1_BL_PIN, &gpio_cfg);
    tkl_gpio_write(BOARD_GC9D01_LCD1_BL_PIN, TUYA_GPIO_LEVEL_HIGH);
    return;
#endif

#if defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
#if !defined(LIBTECH_POP_T5AI_LCD0_ENABLE) || (LIBTECH_POP_T5AI_LCD0_ENABLE != 1)
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };

    tkl_gpio_init(BOARD_LCD_BL_PIN, &gpio_cfg);
    tkl_gpio_write(BOARD_LCD_BL_PIN, TUYA_GPIO_LEVEL_HIGH);
#endif

#if !defined(LIBTECH_POP_T5AI_LCD1_ENABLE) || (LIBTECH_POP_T5AI_LCD1_ENABLE != 1)
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };

    tkl_gpio_init(BOARD_LCD2_BL_PIN, &gpio_cfg);
    tkl_gpio_write(BOARD_LCD2_BL_PIN, TUYA_GPIO_LEVEL_HIGH);
#endif
#endif
}
#endif

OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

    cfg.aec_enable = 1;

    cfg.ai_chn = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits = TKL_AUDIO_DATABITS_16;
    cfg.channel = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME)
    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode = TUYA_GPIO_PUSH_PULL;

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));
#endif

    return rt;
}

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
#if !defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) || (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 != 1) \
    || (defined(LIBTECH_POP_T5AI_LCD0_ENABLE) && (LIBTECH_POP_T5AI_LCD0_ENABLE == 1))
    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width = BOARD_LCD_WIDTH;
    display_cfg.height = BOARD_LCD_HEIGHT;
    display_cfg.x_offset = BOARD_LCD_X_OFFSET;
    display_cfg.y_offset = BOARD_LCD_Y_OFFSET;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1)
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));
#elif defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1)
    tdd_disp_spi_st7735s_set_init_seq(cST7735S_XLT_INIT_SEQ);
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_register(DISPLAY_NAME, &display_cfg));
#elif defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_gc9d01_register((char *)BOARD_DISPLAY_NAME_0, &display_cfg));
#endif
#endif
#endif

#if defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
#if defined(LIBTECH_POP_T5AI_LCD1_ENABLE) && (LIBTECH_POP_T5AI_LCD1_ENABLE == 1)
    DISP_SPI_DEVICE_CFG_T display2_cfg;
    const char *disp2_name = BOARD_DISPLAY_NAME_1;

    /* If LCD0 is disabled, map LCD1 as primary display name to keep UI path available. */
#if !defined(LIBTECH_POP_T5AI_LCD0_ENABLE) || (LIBTECH_POP_T5AI_LCD0_ENABLE != 1)
    disp2_name = BOARD_DISPLAY_NAME_0;
#endif

    memset(&display2_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display2_cfg.bl.type = BOARD_LCD2_BL_TYPE;
    display2_cfg.bl.gpio.pin = BOARD_LCD2_BL_PIN;
    display2_cfg.bl.gpio.active_level = BOARD_LCD2_BL_ACTIVE_LV;

    display2_cfg.width = BOARD_LCD_WIDTH;
    display2_cfg.height = BOARD_LCD_HEIGHT;
    display2_cfg.x_offset = BOARD_LCD_X_OFFSET;
    display2_cfg.y_offset = BOARD_LCD_Y_OFFSET;
    display2_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display2_cfg.rotation = BOARD_LCD_ROTATION;

    display2_cfg.port = BOARD_LCD_SPI2_PORT;
    display2_cfg.spi_clk = BOARD_LCD_SPI2_CLK;
    display2_cfg.cs_pin = BOARD_LCD_SPI2_CS_PIN;
    display2_cfg.dc_pin = BOARD_LCD_SPI2_DC_PIN;
    display2_cfg.rst_pin = BOARD_LCD_SPI2_RST_PIN;

    display2_cfg.power.pin = BOARD_LCD_POWER_PIN;
    display2_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_gc9d01_register((char *)disp2_name, &display2_cfg));
#endif
#endif

    return rt;
}
#endif

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, optional display) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
TUYA_GPIO_LEVEL_E board_lcd_backlight_active_level_get(uint8_t instance)
{
#if defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
    if (instance == 1U) {
        return BOARD_LCD2_BL_ACTIVE_LV;
    }
#endif

    return BOARD_LCD_BL_ACTIVE_LV;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1) \
    || defined(ENABLE_HARDWARE_LCD) && (ENABLE_HARDWARE_LCD != 1)
    __board_gc9d01_backlight_force_off_if_disabled();
#endif

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_led());

#if defined(LIBTECH_POP_T5AI_EX_MODULE_ST7789) && (LIBTECH_POP_T5AI_EX_MODULE_ST7789 == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT) && (LIBTECH_POP_T5AI_EX_MODULE_ST7735S_XLT == 1) \
    || defined(LIBTECH_POP_T5AI_EX_MODULE_GC9D01) && (LIBTECH_POP_T5AI_EX_MODULE_GC9D01 == 1)
    TUYA_CALL_ERR_LOG(__board_register_display());
#endif

    return rt;
}

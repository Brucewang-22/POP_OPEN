/**
 * @file lcd_gc9d01.c
 * @brief GC9D01 双屏独立控制实现
 */

#include "lcd_gc9d01.h"

#include "tal_log.h"
#include "tdd_disp_gc9d01.h"
#include "tdd_disp_type.h"
#include "tdl_display_manage.h"

#define LCD_GC9D01_WIDTH  160
#define LCD_GC9D01_HEIGHT 160
#define LCD_GC9D01_NAME_0 "lcd_gc9d01_0"
#define LCD_GC9D01_NAME_1 "lcd_gc9d01_1"

typedef struct {
    bool inited;
    const char *name;
} LCD_CTX_T;

static LCD_CTX_T sg_lcd_ctx[LCD_INSTANCE_MAX] = {
    [LCD_INSTANCE_0] = {.inited = false, .name = LCD_GC9D01_NAME_0},
    [LCD_INSTANCE_1] = {.inited = false, .name = LCD_GC9D01_NAME_1},
};

static OPERATE_RET __lcd_open_dev(LCD_INSTANCE_E instance, TDL_DISP_HANDLE_T *handle)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_HANDLE_T disp_handle = NULL;

    if (instance >= LCD_INSTANCE_MAX || handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_lcd_ctx[instance].inited) {
        PR_ERR("LCD instance %d not initialized", instance);
        return OPRT_INVALID_PARM;
    }

    disp_handle = tdl_disp_find_dev((char *)sg_lcd_ctx[instance].name);
    if (disp_handle == NULL) {
        PR_ERR("Failed to find display: %s", sg_lcd_ctx[instance].name);
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(disp_handle));
    *handle = disp_handle;

    return OPRT_OK;
}

static OPERATE_RET __lcd_fill(LCD_INSTANCE_E instance, uint16_t color)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_HANDLE_T disp_handle = NULL;
    TDL_DISP_FRAME_BUFF_T frame_buff = {0};
    uint16_t *buffer = NULL;
    uint32_t pixel_count = LCD_GC9D01_WIDTH * LCD_GC9D01_HEIGHT;
    uint32_t buffer_size = pixel_count * sizeof(uint16_t);
    uint32_t i = 0;

    TUYA_CALL_ERR_RETURN(__lcd_open_dev(instance, &disp_handle));

    buffer = (uint16_t *)tal_malloc(buffer_size);
    if (buffer == NULL) {
        PR_ERR("LCD instance %d alloc frame buffer failed", instance);
        tdl_disp_dev_close(disp_handle);
        return OPRT_MALLOC_FAILED;
    }

    for (i = 0; i < pixel_count; i++) {
        buffer[i] = color;
    }

    frame_buff.frame = (uint8_t *)buffer;
    frame_buff.width = LCD_GC9D01_WIDTH;
    frame_buff.height = LCD_GC9D01_HEIGHT;
    frame_buff.fmt = TUYA_PIXEL_FMT_RGB565;
    frame_buff.x_start = 0;
    frame_buff.y_start = 0;
    frame_buff.len = buffer_size;

    TUYA_CALL_ERR_LOG(tdl_disp_dev_flush(disp_handle, &frame_buff));

    tal_free(buffer);
    tdl_disp_dev_close(disp_handle);

    return rt;
}

OPERATE_RET lcd_gc9d01_init(LCD_INSTANCE_E instance, LCD_GC9D01_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_SPI_DEVICE_CFG_T display_cfg = {0};

    if (instance >= LCD_INSTANCE_MAX || cfg == NULL) {
        PR_ERR("Invalid LCD init param, instance=%d, cfg=%p", instance, cfg);
        return OPRT_INVALID_PARM;
    }

    if (sg_lcd_ctx[instance].inited) {
        PR_NOTICE("LCD instance %d already initialized", instance);
        return OPRT_OK;
    }

    display_cfg.width = LCD_GC9D01_WIDTH;
    display_cfg.height = LCD_GC9D01_HEIGHT;
    display_cfg.x_offset = 0;
    display_cfg.y_offset = 0;
    display_cfg.pixel_fmt = TUYA_PIXEL_FMT_RGB565;
    display_cfg.rotation = TUYA_DISPLAY_ROTATION_0;

    display_cfg.port = cfg->spi_port;
    display_cfg.spi_clk = cfg->spi_clk;
    display_cfg.cs_pin = cfg->cs_pin;
    display_cfg.dc_pin = cfg->dc_pin;
    display_cfg.rst_pin = cfg->rst_pin;

    if (cfg->bl_pin == TUYA_GPIO_NUM_MAX) {
        display_cfg.bl.type = TUYA_DISP_BL_TP_NONE;
    } else {
        display_cfg.bl.type = TUYA_DISP_BL_TP_GPIO;
        display_cfg.bl.gpio.pin = cfg->bl_pin;
        display_cfg.bl.gpio.active_level = TUYA_GPIO_LEVEL_HIGH;
    }

    display_cfg.power.pin = TUYA_GPIO_NUM_MAX;
    display_cfg.power.active_level = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_gc9d01_register((char *)sg_lcd_ctx[instance].name, &display_cfg));

    sg_lcd_ctx[instance].inited = true;
    PR_NOTICE("LCD instance %d initialized, name=%s", instance, sg_lcd_ctx[instance].name);

    return rt;
}

OPERATE_RET lcd_gc9d01_turn_on(LCD_INSTANCE_E instance)
{
    return __lcd_fill(instance, 0xFFFF);
}

OPERATE_RET lcd_gc9d01_clear(LCD_INSTANCE_E instance)
{
    return __lcd_fill(instance, 0x0000);
}

OPERATE_RET lcd_gc9d01_fill_color(LCD_INSTANCE_E instance, uint16_t color_rgb565)
{
    return __lcd_fill(instance, color_rgb565);
}

OPERATE_RET lcd_gc9d01_deinit(LCD_INSTANCE_E instance)
{
    if (instance >= LCD_INSTANCE_MAX) {
        PR_ERR("Invalid LCD instance %d", instance);
        return OPRT_INVALID_PARM;
    }

    sg_lcd_ctx[instance].inited = false;
    return OPRT_OK;
}

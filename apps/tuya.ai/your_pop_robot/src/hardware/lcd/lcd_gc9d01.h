/**
 * @file lcd_gc9d01.h
 * @brief GC9D01 双屏独立控制接口
 */

#ifndef __LCD_GC9D01_H__
#define __LCD_GC9D01_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LCD_INSTANCE_0 = 0,
    LCD_INSTANCE_1 = 1,
    LCD_INSTANCE_MAX,
} LCD_INSTANCE_E;

typedef struct {
    TUYA_SPI_NUM_E spi_port;
    TUYA_GPIO_NUM_E cs_pin;
    TUYA_GPIO_NUM_E dc_pin;
    TUYA_GPIO_NUM_E rst_pin;
    TUYA_GPIO_NUM_E bl_pin;
    uint32_t spi_clk;
} LCD_GC9D01_CFG_T;

OPERATE_RET lcd_gc9d01_init(LCD_INSTANCE_E instance, LCD_GC9D01_CFG_T *cfg);
OPERATE_RET lcd_gc9d01_turn_on(LCD_INSTANCE_E instance);
OPERATE_RET lcd_gc9d01_clear(LCD_INSTANCE_E instance);
OPERATE_RET lcd_gc9d01_fill_color(LCD_INSTANCE_E instance, uint16_t color_rgb565);
OPERATE_RET lcd_gc9d01_deinit(LCD_INSTANCE_E instance);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_GC9D01_H__ */

#ifndef LCD_ASSET_TYPES_H
#define LCD_ASSET_TYPES_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;
    uint32_t len;
    const char *name;
} LOCAL_LCD_FRAME_T;

#endif

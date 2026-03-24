/**
 * @file output_hal.h
 * @brief Output HAL mode control (lcd/audio/moto).
 */
#pragma once

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OUTPUT_MODE_1 = 1,
    OUTPUT_MODE_2 = 2,
    OUTPUT_MODE_3 = 3,
} OUTPUT_MODE_E;

OPERATE_RET output_hal_init(void);
OPERATE_RET output_hal_set_lcd_mode(OUTPUT_MODE_E mode);
OPERATE_RET output_hal_set_audio_mode(OUTPUT_MODE_E mode);
OPERATE_RET output_hal_set_moto_mode(OUTPUT_MODE_E mode);

#ifdef __cplusplus
}
#endif


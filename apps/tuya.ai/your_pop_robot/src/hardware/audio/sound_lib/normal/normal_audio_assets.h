#ifndef NORMAL_AUDIO_ASSETS_H
#define NORMAL_AUDIO_ASSETS_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;
    uint32_t len;
    const char *name;
} LOCAL_AUDIO_CLIP_T;

extern const LOCAL_AUDIO_CLIP_T g_normal_audio_clips[];
extern const uint32_t g_normal_audio_clip_count;

#endif

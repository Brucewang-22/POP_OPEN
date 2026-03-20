#ifndef DOUBT_AUDIO_ASSETS_H
#define DOUBT_AUDIO_ASSETS_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;
    uint32_t len;
    const char *name;
} LOCAL_AUDIO_CLIP_T;

extern const LOCAL_AUDIO_CLIP_T g_doubt_audio_clips[];
extern const uint32_t g_doubt_audio_clip_count;

#endif

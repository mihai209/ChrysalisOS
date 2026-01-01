#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_BACKEND_NONE = 0,
    AUDIO_BACKEND_PC_SPEAKER,
    AUDIO_BACKEND_AC97,
    AUDIO_BACKEND_HDA
} audio_backend_t;

void audio_init();
audio_backend_t audio_backend();

void audio_beep(uint32_t freq, uint32_t ms);

/* viitor */
int audio_play_pcm(const void* data, uint32_t size, uint32_t rate);

#ifdef __cplusplus
}
#endif

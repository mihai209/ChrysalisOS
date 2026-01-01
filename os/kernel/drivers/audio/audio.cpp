#include "audio.h"
#include "../speaker/speaker.h"

static audio_backend_t backend = AUDIO_BACKEND_NONE;

void audio_init()
{
    // deocamdată avem doar PC speaker
    backend = AUDIO_BACKEND_PC_SPEAKER;
}

audio_backend_t audio_backend()
{
    return backend;
}

void audio_beep(uint32_t freq, uint32_t ms)
{
    if (backend == AUDIO_BACKEND_PC_SPEAKER)
        speaker_beep(freq, ms);
}

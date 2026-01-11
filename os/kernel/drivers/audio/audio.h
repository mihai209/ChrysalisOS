#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void audio_init();
void audio_write(void* data, int size);

#ifdef __cplusplus
}
#endif

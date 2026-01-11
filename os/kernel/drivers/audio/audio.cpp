#include "audio.h"
#include "../../mem/kmalloc.h"
#include "../../string.h"

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

/* Audio Ring Buffer */
#define AUDIO_BUFFER_SIZE (64 * 1024)
static uint8_t* audio_buffer = NULL;
static volatile uint32_t write_head = 0;
static volatile uint32_t read_head = 0;

extern "C" void audio_init() {
    audio_buffer = (uint8_t*)kmalloc(AUDIO_BUFFER_SIZE);
    if (audio_buffer) {
        memset(audio_buffer, 0, AUDIO_BUFFER_SIZE);
        serial("[AUDIO] Initialized ring buffer (%d bytes)\n", AUDIO_BUFFER_SIZE);
    } else {
        serial("[AUDIO] Failed to allocate buffer\n");
    }
    
    /* TODO: Initialize SB16 or AC97 here */
}

extern "C" void audio_write(void* data, int size) {
    if (!audio_buffer) return;
    
    const uint8_t* src = (const uint8_t*)data;
    for (int i = 0; i < size; i++) {
        uint32_t next_head = (write_head + 1) % AUDIO_BUFFER_SIZE;
        
        /* If buffer is not full, write byte */
        if (next_head != read_head) {
            audio_buffer[write_head] = src[i];
            write_head = next_head;
        }
    }
}

#include "doomgeneric.h"
#include "../../../ui/wm/wm.h"
#include "../../../ui/flyui/draw.h"
#include "../../../ui/flyui/theme.h"
#include "../../../input/input.h"
#include "../../../mem/kmalloc.h"
#include "../../../cmds/fat.h"
#include "../../../string.h"
#include "../../../terminal.h"
#include "../../../apps/app_manager.h"

/* Fix boolean conflict between stdbool.h (OS) and doomtype.h (Doom) */
#ifdef false
#undef false
#endif
#ifdef true
#undef true
#endif

#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"

/* Networking stubs for Doom */
int drone = 0;
int net_client_connected = 0;

extern void serial(const char *fmt, ...);
extern int sprintf(char *str, const char *format, ...);

/* Timer helper */
extern uint64_t hpet_time_ms(void);
uint32_t timer_get_ticks(void) { return (uint32_t)(hpet_time_ms() / 10); }

static window_t* doom_window = NULL;

#define TITLE_BAR_HEIGHT 24

/* Input Queue */
#define KEY_QUEUE_SIZE 32
static struct {
    int pressed;
    unsigned char key;
} key_queue[KEY_QUEUE_SIZE];
static int key_head = 0;
static int key_tail = 0;

/* Mouse State for Logging */
static int mouse_acc_x = 0;
static int mouse_acc_y = 0;

/*
 * Pushes a key event to the DoomGeneric queue.
 * pressed: 1=down, 0=up
 */
void doom_key_push(int pressed, unsigned char key) {
    int next = (key_head + 1) % KEY_QUEUE_SIZE;
    if (next != key_tail) {
        key_queue[key_head].pressed = pressed;
        key_queue[key_head].key = key;
        key_head = next;
    }
}

/* Exposed Input API for Wrapper */
void DG_KeyDown(int key) {
    serial("[DOOM] KeyDown: %d\n", key);
    doom_key_push(1, (unsigned char)key);
}

void DG_KeyUp(int key) {
    /* DoomGeneric often only needs KeyDown for some things, but KeyUp is essential for movement */
    doom_key_push(0, (unsigned char)key);
}

void DG_MouseMove(int dx, int dy) {
    mouse_acc_x += dx;
    mouse_acc_y += dy;
    serial("[DOOM] MouseMove dx=%d dy=%d\n", dx, dy);
    /* Note: To actually move the player, i_input.c in DoomGeneric needs to read this.
       For now, we log as requested. */
}

void DG_MouseButton(int button, int pressed) {
    serial("[DOOM] MouseButton btn=%d pressed=%d\n", button, pressed);
    /* Map mouse buttons if needed by engine */
}

/* SSE Optimization Helpers */
static void enable_sse(void) {
    uint32_t cr0, cr4;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); /* Clear EM */
    cr0 |= (1 << 1);  /* Set MP */
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  /* Set OSFXSR */
    cr4 |= (1 << 10); /* Set OSXMMEXCPT */
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}

__attribute__((target("sse2")))
static void fast_blit_sse(void* dst, const void* src, size_t size) {
    const char* s = (const char*)src;
    char* d = (char*)dst;
    
    size_t n = size / 64;
    size_t r = size % 64;
    
    for (size_t i = 0; i < n; i++) {
        asm volatile (
            "movdqu (%0), %%xmm0\n"
            "movdqu 16(%0), %%xmm1\n"
            "movdqu 32(%0), %%xmm2\n"
            "movdqu 48(%0), %%xmm3\n"
            "movdqu %%xmm0, (%1)\n"
            "movdqu %%xmm1, 16(%1)\n"
            "movdqu %%xmm2, 32(%1)\n"
            "movdqu %%xmm3, 48(%1)\n"
            : 
            : "r"(s), "r"(d) 
            : "memory", "xmm0", "xmm1", "xmm2", "xmm3"
        );
        s += 64;
        d += 64;
    }
    
    if (r > 0) memcpy(d, s, r);
}

/* --- Sound Mixer Implementation --- */

/* OS Audio Driver API */
extern void audio_write(void* data, int size);

#define MIX_CHANNELS 16
#define DOOM_FREQ 11025
#define OUT_FREQ 48000
#define MIX_BUFFER_SAMPLES 1024 /* ~21ms at 48kHz */

typedef struct {
    int active;
    const uint8_t* data;
    uint32_t length;
    uint32_t position; /* 16.16 fixed point */
    int volume;        /* 0-127 */
    int pan;           /* 0-255 (128=center) */
    uint32_t step;     /* 16.16 fixed point */
} mixer_channel_t;

static mixer_channel_t mix_channels[MIX_CHANNELS];
static boolean sound_enabled = false;
static int16_t mix_buffer[MIX_BUFFER_SAMPLES * 2]; /* Stereo */

static boolean S_Init(boolean use_sfx_prefix) {
    (void)use_sfx_prefix;
    memset(mix_channels, 0, sizeof(mix_channels));
    sound_enabled = true;
    serial("[DOOM] Sound Mixer Initialized (48kHz Stereo)\n");
    return true;
}

static void S_Shutdown(void) {
    sound_enabled = false;
}

static int S_GetSfxLumpNum(sfxinfo_t *sfx) {
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

static void S_UpdateSoundParams(int channel, int vol, int sep) {
    if (channel < 0 || channel >= MIX_CHANNELS) return;
    mix_channels[channel].volume = vol;
    mix_channels[channel].pan = sep;
}

static int S_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep) {
    if (!sound_enabled || channel < 0 || channel >= MIX_CHANNELS) return -1;

    /* Load lump if not cached */
    if (!sfxinfo->driver_data) {
        int lump = sfxinfo->lumpnum;
        if (lump < 0) lump = S_GetSfxLumpNum(sfxinfo);
        
        /* Cache as static to avoid reloading */
        sfxinfo->driver_data = W_CacheLumpNum(lump, PU_STATIC);
    }

    uint8_t* raw = (uint8_t*)sfxinfo->driver_data;
    if (!raw) return -1;

    /* Parse DMX Header */
    /* uint16_t type = *(uint16_t*)raw; */
    uint16_t rate = *(uint16_t*)(raw + 2);
    uint32_t len  = *(uint32_t*)(raw + 4);
    
    /* Sanity check */
    if (len > 256 * 1024) return -1; 

    mix_channels[channel].active = 1;
    mix_channels[channel].data = raw + 8; /* Skip 8-byte header */
    mix_channels[channel].length = len;
    mix_channels[channel].position = 0;
    mix_channels[channel].volume = vol;
    mix_channels[channel].pan = sep;
    
    /* Calculate step for resampling (16.16 fixed point) */
    /* step = (src_rate / dst_rate) * 65536 */
    mix_channels[channel].step = (rate * 65536) / OUT_FREQ;

    return channel;
}

static void S_StopSound(int channel) {
    if (channel >= 0 && channel < MIX_CHANNELS) {
        mix_channels[channel].active = 0;
    }
}

static boolean S_SoundIsPlaying(int channel) {
    if (channel >= 0 && channel < MIX_CHANNELS) {
        return mix_channels[channel].active;
    }
    return false;
}

static void S_Update(void) {
    if (!sound_enabled) return;

    /* Mix a chunk of audio */
    memset(mix_buffer, 0, sizeof(mix_buffer));

    for (int i = 0; i < MIX_CHANNELS; i++) {
        if (!mix_channels[i].active) continue;

        mixer_channel_t* ch = &mix_channels[i];
        
        for (int j = 0; j < MIX_BUFFER_SAMPLES; j++) {
            uint32_t pos_int = ch->position >> 16;
            
            if (pos_int >= ch->length) {
                ch->active = 0;
                break;
            }

            /* Get sample (8-bit unsigned -> 16-bit signed) */
            int16_t sample = ((int16_t)ch->data[pos_int] - 128) * 256;
            
            /* Apply Volume */
            sample = (sample * ch->volume) / 127;

            /* Stereo Panning (0=Left, 128=Center, 255=Right) */
            int left_vol = (255 - ch->pan) + 1;
            int right_vol = ch->pan + 1;
            
            /* Mix Left */
            int32_t out_l = mix_buffer[j*2] + ((sample * left_vol) >> 8);
            if (out_l > 32767) out_l = 32767;
            if (out_l < -32768) out_l = -32768;
            mix_buffer[j*2] = (int16_t)out_l;

            /* Mix Right */
            int32_t out_r = mix_buffer[j*2+1] + ((sample * right_vol) >> 8);
            if (out_r > 32767) out_r = 32767;
            if (out_r < -32768) out_r = -32768;
            mix_buffer[j*2+1] = (int16_t)out_r;

            ch->position += ch->step;
        }
    }

    /* Send to OS Driver */
    audio_write(mix_buffer, sizeof(mix_buffer));
}

/* Define the module structure expected by i_sound.c */
sound_module_t DG_sound_module = {
    NULL, /* sound_devices */
    0,    /* num_sound_devices */
    S_Init,
    S_Shutdown,
    S_GetSfxLumpNum,
    S_Update,
    S_UpdateSoundParams,
    S_StartSound,
    S_StopSound,
    S_SoundIsPlaying,
    NULL, /* CacheSounds (optional) */
};

/* --- DoomGeneric Implementation --- */

void DG_Init() {
    enable_sse();

    /* Create Window */
    /* Add space for Title Bar */
    surface_t* s = surface_create(DOOMGENERIC_RESX, DOOMGENERIC_RESY + TITLE_BAR_HEIGHT);
    if (!s) return;
    
    /* Draw Title Bar */
    fly_draw_rect_fill(s, 0, 0, DOOMGENERIC_RESX, TITLE_BAR_HEIGHT, 0xFF000000);
    fly_draw_text(s, 10, 4, "DOOM", 0xFFFFFFFF);
    
    /* Draw Close Button [X] */
    int bx = DOOMGENERIC_RESX - 20;
    fly_draw_rect_fill(s, bx, 4, 16, 16, 0xFFC0C0C0);
    fly_draw_text(s, bx + 4, 4, "X", 0xFF000000);

    /* Center on screen */
    doom_window = wm_create_window(s, 100, 100);
    
    /* Register app for Task Manager */
    app_register("DOOM", doom_window);
    
    terminal_printf("[DOOM] Window initialized.\n");
}

void DG_DrawFrame() {
    if (!doom_window) return;
    
    /* Blit buffer to window surface */
    /* DoomGeneric uses XRGB or similar. Chrysalis uses ARGB/BGRA (32bpp). */
    /* Assuming pixel_t is uint32_t (defined in doomgeneric.h) */
    /* DG_ScreenBuffer is allocated in doomgeneric.c */

    if (!DG_ScreenBuffer) return;

    /* Blit to surface, offset by TITLE_BAR_HEIGHT */
    uint32_t* dst = doom_window->surface->pixels + (TITLE_BAR_HEIGHT * DOOMGENERIC_RESX);
    fast_blit_sse(dst, DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
    
    wm_mark_dirty();
}

void DG_SleepMs(uint32_t ms) {
    /* Busy wait or yield (we don't have sleep() exposed easily here without headers) */
    /* Using a simple loop for now as we are in a cooperative loop anyway */
    uint32_t start = timer_get_ticks();
    while ((timer_get_ticks() - start) * 10 < ms) {
        asm volatile("nop");
    }
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)hpet_time_ms();
}

int DG_GetKey(int* pressed, unsigned char* key) {
    if (key_head == key_tail) return 0;
    
    *pressed = key_queue[key_tail].pressed;
    *key = key_queue[key_tail].key;
    
    key_tail = (key_tail + 1) % KEY_QUEUE_SIZE;
    return 1;
}

void DG_SetWindowTitle(const char * title) {
    (void)title;
    /* Window title is drawn by WM, currently static or managed by app_create */
}

/* --- Window Access for App Wrapper --- */
window_t* doom_get_window(void) {
    return doom_window;
}

void doom_destroy_window(void) {
    if (doom_window) {
        serial("[DOOM] Exiting cleanly\n");
        wm_destroy_window(doom_window);
        app_unregister(doom_window);
        doom_window = NULL;
    }
}

/* --- Minimal C Library Wrappers for Doom --- */
/* These are needed because Doom uses stdio/stdlib */

void* malloc(size_t size) { return kmalloc(size); }
void* calloc(size_t nmemb, size_t size) {
    void* ptr = kmalloc(nmemb * size);
    if (ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}
void* realloc(void* ptr, size_t size) {

    void* new_ptr = kmalloc(size);
    if (ptr && new_ptr) {
        memcpy(new_ptr, ptr, size); // DANGEROUS: might read OOB
        kfree(ptr);
    }
    return new_ptr;
}
void free(void* ptr) { kfree(ptr); }

int printf(const char* fmt, ...) {

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    terminal_vprintf(fmt, &args);
    __builtin_va_end(args);
    return 0;
}

int puts(const char* s) {
    terminal_writestring(s);
    terminal_putchar('\n');
    return 0;
}

/* File I/O functions moved to kernel/posix_stubs.c */
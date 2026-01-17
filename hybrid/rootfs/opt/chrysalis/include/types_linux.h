/*
 * types_linux.h
 * ChrysalisOS Linux Compatibility - Type Definitions
 * 
 * Maps Chrysalis types to standard Linux/POSIX types
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

/* Basic types - Chrysalis style names, Linux values */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint32_t dword;
typedef uint16_t word;
typedef uint8_t  byte;

typedef unsigned int uint;

/* Process and thread types */
typedef pid_t    process_id_t;
typedef process_id_t process_t;  /* Will be PID in Linux */

typedef struct {
    pid_t pid;
    char name[32];
    uint32_t priority;
    uint32_t state;
} task_t;

/* Memory types */
typedef void*    virtual_addr_t;
typedef uint32_t physical_addr_t;

typedef struct {
    virtual_addr_t  start;
    physical_addr_t phys;
    size_t          size;
    uint32_t        flags;
} memory_page_t;

/* File system types */
typedef int      file_handle_t;
typedef int      file_descriptor_t;  /* Same as Linux fd */

typedef struct {
    char filename[256];
    uint32_t file_size;
    uint32_t create_time;
    uint32_t modify_time;
    uint16_t permissions;
    uint32_t inode;
} file_stat_t;

/* Device types */
typedef uint32_t device_id_t;
typedef int      device_handle_t;

/* Interrupt/IRQ types */
typedef uint8_t  irq_num_t;
typedef void (*irq_handler_t)(void);

/* Port I/O types */
typedef uint16_t io_port_t;

/* Frame buffer types */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;        /* Bits per pixel */
    uint8_t* framebuffer;
    uint32_t size;
} framebuffer_t;

/* Color type (ARGB format) */
typedef uint32_t color_t;
#define COLOR_MAKE(r, g, b) ((0xFF << 24) | ((r) << 16) | ((g) << 8) | (b))
#define COLOR_RED    0xFFFF0000
#define COLOR_GREEN  0xFF00FF00
#define COLOR_BLUE   0xFF0000FF
#define COLOR_WHITE  0xFFFFFFFF
#define COLOR_BLACK  0xFF000000

/* Kernel return codes */
typedef int kresult_t;
#define K_OK              0
#define K_ERROR          -1
#define K_ENOENT         -2
#define K_EACCES         -3
#define K_ENOMEM         -4
#define K_EBUSY          -5
#define K_ENOTFOUND      -6

/* Task states */
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING = 1,
    TASK_BLOCKED = 2,
    TASK_TERMINATED = 3,
    TASK_SLEEPING = 4
} task_state_t;

/* Flags for common operations */
#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_CREAT    0x40
#define O_APPEND   0x400
#define O_TRUNC    0x200

/* Register structure (i386) */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
} registers_t;

/* Keyboard key codes */
typedef enum {
    KEY_NULL = 0,
    KEY_ESC = 1,
    KEY_BACKSPACE = 14,
    KEY_TAB = 15,
    KEY_ENTER = 28,
    KEY_LSHIFT = 42,
    KEY_RSHIFT = 54,
    KEY_LCTRL = 29,
    KEY_RCTRL = 61,
    KEY_LALT = 56,
    KEY_RALT = 100,
    KEY_SPACE = 57,
    KEY_F1 = 59,
    KEY_F2 = 60,
    KEY_F12 = 88,
} key_code_t;

/* Mouse button */
typedef enum {
    MOUSE_LEFT = 1,
    MOUSE_MIDDLE = 2,
    MOUSE_RIGHT = 4
} mouse_button_t;

typedef struct {
    int x, y;
    mouse_button_t buttons;
} mouse_state_t;

/* Event types */
typedef enum {
    EVENT_KEY_PRESS = 1,
    EVENT_KEY_RELEASE = 2,
    EVENT_MOUSE_MOVE = 3,
    EVENT_MOUSE_CLICK = 4,
    EVENT_TIMER = 5,
    EVENT_SIGNAL = 6
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t timestamp;
    uint32_t data;
} event_t;

/* Window handle */
typedef void* window_t;

/* NULL pointer constant */
#ifndef NULL
#define NULL ((void*)0)
#endif /* TYPES_LINUX_H */

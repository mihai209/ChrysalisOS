// kernel/cmds/registry.cpp
#include "registry.h"

#include "clear.h"
#include "reboot.h"
#include "shutdown.h"
#include "ls.h"
#include "cat.h"
#include "touch.h"
#include "date.h"
#include "beep.h"
#include "play.h"
#include "uptime.h"
#include "ticks.h"
#include "help.h"
#include "disk.h"   // stil vechi: void cmd_disk(const char*)
#include "fat.h"    // stil nou: int cmd_fat(int, char**)
#include "vfs.h"
#include "pmm.h"
#include "login.h"
#include "crash.h"
#include "mem.h"
#include "credits.h"
#include "echo.h"
#include "fortune.h"
#include "buildinfo.h"
#include "sysfetch.h"
#include "chrysver.h"

// Definiri manuale freestanding
#ifndef NULL
#define NULL 0          // în loc de ((void*)0) → evită eroarea de conversie
#endif

typedef unsigned long size_t;

// Implementări manuale (freestanding-friendly)
static size_t k_strlen(const char* s) {
    const char* p = s;
    while (*p) ++p;
    return (size_t)(p - s);
}

static void* k_memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

/* Wrapper generic pentru comenzile vechi */
static int old_style_wrapper(void (*old_func)(const char*), int argc, char **argv)
{
    if (argc <= 1) {
        old_func(NULL);   // acum NULL este 0 → conversie implicită validă la const char*
        return 0;
    }

    size_t total_len = 0;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) total_len += 1;                // spațiu
        total_len += k_strlen(argv[i]);
    }

    static char args_buffer[256];
    char* p = args_buffer;

    for (int i = 1; i < argc; ++i) {
        if (i > 1) *p++ = ' ';

        size_t len = k_strlen(argv[i]);
        k_memcpy(p, argv[i], len);
        p += len;
    }
    *p = '\0';

    old_func(args_buffer);
    return 0;
}

/* Wrapper-e pentru toate comenzile vechi */
static int wrap_cmd_beep(int argc, char **argv)     { return old_style_wrapper(cmd_beep,     argc, argv); }
static int wrap_cmd_cat(int argc, char **argv)      { return old_style_wrapper(cmd_cat,      argc, argv); }
static int wrap_cmd_clear(int argc, char **argv)    { return old_style_wrapper(cmd_clear,    argc, argv); }
static int wrap_cmd_date(int argc, char **argv)     { return old_style_wrapper(cmd_date,     argc, argv); }
static int wrap_cmd_disk(int argc, char **argv)     { return old_style_wrapper(cmd_disk,     argc, argv); }
static int wrap_cmd_help(int argc, char **argv)     { return old_style_wrapper(cmd_help,     argc, argv); }
static int wrap_cmd_ls(int argc, char **argv)       { return old_style_wrapper(cmd_ls,       argc, argv); }
static int wrap_cmd_play(int argc, char **argv)     { return old_style_wrapper(cmd_play,     argc, argv); }
static int wrap_cmd_reboot(int argc, char **argv)   { return old_style_wrapper(cmd_reboot,   argc, argv); }
static int wrap_cmd_shutdown(int argc, char **argv) { return old_style_wrapper(cmd_shutdown, argc, argv); }
static int wrap_cmd_ticks(int argc, char **argv)    { return old_style_wrapper(cmd_ticks,    argc, argv); }
static int wrap_cmd_touch(int argc, char **argv)    { return old_style_wrapper(cmd_touch,    argc, argv); }
static int wrap_cmd_uptime(int argc, char **argv)   { return old_style_wrapper(cmd_uptime,   argc, argv); }
//static int wrap_cmd_login_main(int argc, char **argv) { return old_style_wrapper(cmd_login_main, argc, argv); }
/* Tabelul final de comenzi */
Command command_table[] = {
    { "buildinfo", (command_fn)cmd_buildinfo },
    { "beep",     (command_fn)wrap_cmd_beep },
    { "chrysver", (command_fn)cmd_chrysver },
    { "crash",    (command_fn)cmd_crash},
    { "cat",      (command_fn)wrap_cmd_cat },
    { "clear",    (command_fn)wrap_cmd_clear },
    { "credits",  (command_fn)cmd_credits},
    { "date",     (command_fn)wrap_cmd_date },
    { "disk",     (command_fn)wrap_cmd_disk },     // stil vechi → wrapper
    { "exit",     (command_fn)wrap_cmd_shutdown},
    { "echo", (command_fn)cmd_echo },
    { "fortune", (command_fn)cmd_fortune },
    { "fat",      (command_fn)cmd_fat },           // stil nou → direct
    { "help",     (command_fn)wrap_cmd_help },
    { "ls",       (command_fn)wrap_cmd_ls },
    { "login",    (command_fn)cmd_login_main},
    { "mem",      (command_fn)cmd_mem},
    { "pmm",      (command_fn)cmd_pmm},
    { "play",     (command_fn)wrap_cmd_play },
    { "reboot",   (command_fn)wrap_cmd_reboot },
    { "sysfetch", (command_fn)cmd_sysfetch },
    { "shutdown", (command_fn)wrap_cmd_shutdown },
    { "ticks",    (command_fn)wrap_cmd_ticks },
    { "touch",    (command_fn)wrap_cmd_touch },
    { "uptime",   (command_fn)wrap_cmd_uptime },
    { "vfs",      (command_fn)cmd_vfs },

};

int command_count = sizeof(command_table) / sizeof(Command);
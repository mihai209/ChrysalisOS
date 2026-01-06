// kernel/cmds/registry.cpp
// Registry of shell commands for Chrysalis OS
// Clean, freestanding-friendly, wrappers based on header prototypes.

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
#include "disk.h"
#include "fat.h"
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
#include "elf.h"
#include "elf_debug.h"
#include "elf_crash.h"

// Minimal freestanding helpers (no libc)
typedef unsigned long size_t;

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

/*
 * Wrappers:
 *
 * - old-style commands (headers: void cmd_xxx(const char*))
 *   Adapt to shell API int fn(int argc, char** argv)
 *
 * - new-style void commands (headers: void cmd_xxx(int,char**))
 *   Wrap to return int (0)
 *
 * - new-style int commands (headers: int cmd_xxx(int,char**))
 *   Call directly (no wrapper necessary), but we provide thin wrapper to keep table uniform.
 */

/* ----- Generic wrapper implementations ----- */

/* old-style adapter: adapts void func(const char*) to int(int,char**) */
static int wrap_old_style(void (*old_func)(const char*), int argc, char **argv)
{
    if (argc <= 1) {
        old_func(0);
        return 0;
    }

    /* build args string in a small stack buffer */
    char args_buffer[256];
    char *p = args_buffer;

    for (int i = 1; i < argc; ++i) {
        if (i > 1) { *p++ = ' '; }
        const char *src = argv[i];
        while (*src) {
            *p++ = *src++;
            /* avoid overflowing buffer */
            if ((size_t)(p - args_buffer) >= sizeof(args_buffer) - 1) break;
        }
        if ((size_t)(p - args_buffer) >= sizeof(args_buffer) - 1) break;
    }
    *p = '\0';

    old_func(args_buffer);
    return 0;
}

/* wrapper for new-style void functions */
static int wrap_new_void(void (*fn)(int, char**), int argc, char **argv) {
    fn(argc, argv);
    return 0;
}

/* wrapper for new-style int functions (calls and returns result) */
static int wrap_new_int(int (*fn)(int, char**), int argc, char **argv) {
    return fn(argc, argv);
}

/* ----- Generate wrappers for each command according to header prototypes ----- */

/* Old-style commands (headers: void cmd_xxx(const char*)) */
static int wrap_cmd_beep(int argc, char **argv)      { return wrap_old_style(cmd_beep,      argc, argv); }
static int wrap_cmd_buildinfo(int argc, char **argv) { return wrap_old_style(cmd_buildinfo, argc, argv); }
static int wrap_cmd_chrysver(int argc, char **argv)  { return wrap_old_style(cmd_chrysver,  argc, argv); }
static int wrap_cmd_clear(int argc, char **argv)     { return wrap_old_style(cmd_clear,     argc, argv); }
static int wrap_cmd_crash(int argc, char **argv)     { return wrap_old_style(cmd_crash,     argc, argv); }
static int wrap_cmd_credits(int argc, char **argv)   { return wrap_old_style(cmd_credits,   argc, argv); }
static int wrap_cmd_date(int argc, char **argv)      { return wrap_old_style(cmd_date,      argc, argv); }
static int wrap_cmd_echo(int argc, char **argv)      { return wrap_old_style(cmd_echo,      argc, argv); }
static int wrap_cmd_fortune(int argc, char **argv)   { return wrap_old_style(cmd_fortune,   argc, argv); }
static int wrap_cmd_help(int argc, char **argv)      { return wrap_old_style(cmd_help,      argc, argv); }
static int wrap_cmd_play(int argc, char **argv)      { return wrap_old_style(cmd_play,      argc, argv); }
static int wrap_cmd_reboot(int argc, char **argv)    { return wrap_old_style(cmd_reboot,    argc, argv); }
static int wrap_cmd_shutdown(int argc, char **argv)  { return wrap_old_style(cmd_shutdown,  argc, argv); }
static int wrap_cmd_sysfetch(int argc, char **argv)  { return wrap_old_style(cmd_sysfetch,  argc, argv); }
static int wrap_cmd_ticks(int argc, char **argv)     { return wrap_old_style(cmd_ticks,     argc, argv); }
static int wrap_cmd_touch(int argc, char **argv)     { return wrap_old_style(cmd_touch,     argc, argv); }
static int wrap_cmd_uptime(int argc, char **argv)    { return wrap_old_style(cmd_uptime,    argc, argv); }

/* login and mem headers declare old-style; use old wrapper (if implementations are actually new-style you'll need to migrate those implementations) */
static int wrap_cmd_login(int argc, char **argv)     { return wrap_old_style(cmd_login_main, argc, argv); }
static int wrap_cmd_mem(int argc, char **argv)       { return wrap_old_style(cmd_mem,       argc, argv); }

/* ----- New-style commands (void) according to headers ----- */
static int wrap_cmd_cat(int argc, char **argv)       { return wrap_new_void(cmd_cat, argc, argv); }   /* void cmd_cat(int,char**) */
static int wrap_cmd_disk(int argc, char **argv)      { return wrap_new_void(cmd_disk, argc, argv); }  /* void cmd_disk(int,char**) */
static int wrap_cmd_ls(int argc, char **argv)        { return wrap_new_void(cmd_ls, argc, argv); }    /* void cmd_ls(int,char**) */
static int wrap_cmd_vfs(int argc, char **argv)       { return wrap_new_void(cmd_vfs, argc, argv); }   /* void cmd_vfs(int,char**) */

/* ----- New-style commands (int) according to headers ----- */
static int wrap_cmd_fat(int argc, char **argv)       { return wrap_new_int(cmd_fat, argc, argv); }        /* int cmd_fat(int,char**) */
static int wrap_cmd_elf(int argc, char **argv)       { return wrap_new_int(cmd_elf, argc, argv); }        /* int cmd_elf(int,char**) */
static int wrap_cmd_elf_debug(int argc, char **argv) { return wrap_new_int(cmd_elf_debug, argc, argv); }  /* int cmd_elf_debug(int,char**) */
static int wrap_cmd_elf_crash(int argc, char **argv) { return wrap_new_int(cmd_elf_crash, argc, argv); }  /* int cmd_elf_crash(int,char**) */
static int wrap_cmd_pmm(int argc, char **argv)       { return wrap_new_int(cmd_pmm, argc, argv); }        /* int cmd_pmm(int,char**) */

/* ----- Final command table ----- */
/* Command typedef (from registry.h) assumed: typedef struct { const char* name; command_fn fn; } Command; */
Command command_table[] = {
    { "buildinfo", wrap_cmd_buildinfo },
    { "beep",      wrap_cmd_beep },
    { "chrysver",  wrap_cmd_chrysver },
    { "crash",     wrap_cmd_crash },
    { "cat",       wrap_cmd_cat },
    { "clear",     wrap_cmd_clear },
    { "credits",   wrap_cmd_credits },
    { "date",      wrap_cmd_date },
    { "disk",      wrap_cmd_disk },
    { "echo",      wrap_cmd_echo },
    { "elf",       wrap_cmd_elf },
    { "elf-debug", wrap_cmd_elf_debug },
    { "elf-crash", wrap_cmd_elf_crash },
    { "exit",      wrap_cmd_shutdown },
    { "fat",       wrap_cmd_fat },
    { "fortune",   wrap_cmd_fortune },
    { "help",      wrap_cmd_help },
    { "ls",        wrap_cmd_ls },
    { "login",     wrap_cmd_login },
    { "mem",       wrap_cmd_mem },
    { "pmm",       wrap_cmd_pmm },
    { "play",      wrap_cmd_play },
    { "reboot",    wrap_cmd_reboot },
    { "shutdown",  wrap_cmd_shutdown },
    { "sysfetch",  wrap_cmd_sysfetch },
    { "ticks",     wrap_cmd_ticks },
    { "touch",     wrap_cmd_touch },
    { "uptime",    wrap_cmd_uptime },
    { "vfs",       wrap_cmd_vfs },
};

int command_count = sizeof(command_table) / sizeof(Command);

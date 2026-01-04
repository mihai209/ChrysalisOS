// kernel/arch/i386/syscall_dispatch.c
#include <stdint.h>
#include "../../terminal.h"

/* === syscall numbers === */
#define SYS_WRITE  1

/* === helper intern === */
static int sys_write(const char* s)
{
    if (!s) {
        terminal_writestring("[sys_write] null pointer\n");
        return -1;
    }

    terminal_writestring(s);
    return 0;
}

/*
 * Dispatcher principal
 * ABI:
 *   eax = syscall number
 *   ebx = arg1
 *
 * return:
 *   eax = result
 */
#ifdef __cplusplus
extern "C" {
#endif

int syscall_dispatch(uint32_t num, uint32_t arg1)
{
    terminal_printf("[syscall] num=%u arg=%x\n", num, arg1);

    switch (num) {
        case SYS_WRITE:
            return sys_write((const char*)arg1);

        default:
            terminal_writestring("[syscall] invalid syscall\n");
            return -1;
    }
}

#ifdef __cplusplus
}
#endif

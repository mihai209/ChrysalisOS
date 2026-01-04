#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void syscall_init(void);
int syscall_dispatch(uint32_t num, uint32_t arg1);

#ifdef __cplusplus
}
#endif

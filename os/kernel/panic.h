#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panic / crash helper */
void panic(const char *message);

#ifdef __cplusplus
}
#endif

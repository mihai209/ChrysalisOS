#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t dns_resolve(const char* domain);

#ifdef __cplusplus
}
#endif
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void prng_seed(uint32_t seed);
uint32_t prng_next(void);

#ifdef __cplusplus
}
#endif
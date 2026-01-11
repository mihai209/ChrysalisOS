#include "prng.h"

static uint32_t state = 0xDEADBEEF;

void prng_seed(uint32_t seed) {
    if (seed == 0) seed = 0xDEADBEEF;
    state = seed;
}

uint32_t prng_next(void) {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}
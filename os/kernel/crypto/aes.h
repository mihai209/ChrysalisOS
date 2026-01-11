#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AES-128 context */
typedef struct {
    uint32_t round_key[44];
} aes128_ctx_t;

void aes128_init(aes128_ctx_t* ctx, const uint8_t* key);
void aes128_encrypt_block(const aes128_ctx_t* ctx, const uint8_t* in, uint8_t* out);

#ifdef __cplusplus
}
#endif
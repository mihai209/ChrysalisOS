#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TLS_STATE_CLOSED,
    TLS_STATE_HANDSHAKE,
    TLS_STATE_CONNECTED,
    TLS_STATE_ERROR
} tls_state_t;

typedef struct {
    tls_state_t state;
    uint32_t ip;
    uint16_t port;
} tls_context_t;

/* Initialize a TLS context */
void tls_init_context(tls_context_t* ctx, uint32_t ip, uint16_t port);

/* Create Client Hello packet (returns length) */
int tls_create_client_hello(tls_context_t* ctx, const char* hostname, uint8_t* buf, size_t max_len);

/* Handle received TCP data for TLS */
/* Returns 0 on success, -1 on error */
int tls_handle_rx(tls_context_t* ctx, const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
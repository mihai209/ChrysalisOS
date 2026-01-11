#include "tls.h"
#include "../mm/kmalloc.h"
#include "../string.h"
#include "../crypto/prng.h"

/* TLS Constants */
#define TLS_RT_CHANGE_CIPHER_SPEC 20
#define TLS_RT_ALERT              21
#define TLS_RT_HANDSHAKE          22
#define TLS_RT_APPLICATION_DATA   23

/* Handshake Types */
#define TLS_HT_HELLO_REQUEST       0
#define TLS_HT_CLIENT_HELLO        1
#define TLS_HT_SERVER_HELLO        2
#define TLS_HT_CERTIFICATE         11
#define TLS_HT_SERVER_KEY_EXCHANGE 12
#define TLS_HT_CERTIFICATE_REQUEST 13
#define TLS_HT_SERVER_HELLO_DONE   14
#define TLS_HT_CERTIFICATE_VERIFY  15
#define TLS_HT_CLIENT_KEY_EXCHANGE 16
#define TLS_HT_FINISHED            20

extern void serial(const char *fmt, ...);
extern uint64_t hpet_time_ms(void);

void tls_init_context(tls_context_t* ctx, uint32_t ip, uint16_t port) {
    memset(ctx, 0, sizeof(tls_context_t));
    ctx->state = TLS_STATE_HANDSHAKE;
    ctx->ip = ip;
    ctx->port = port;
}

int tls_create_client_hello(tls_context_t* ctx, const char* hostname, uint8_t* buf, size_t max_len) {
    (void)ctx;
    if (max_len < 128) return -1;

    uint8_t* p = buf;
    
    /* 1. Record Layer Header */
    *p++ = TLS_RT_HANDSHAKE;
    *p++ = 0x03; *p++ = 0x01; /* TLS 1.0 for compatibility in record layer */
    uint8_t* len_ptr = p;
    p += 2; /* Length placeholder */

    /* 2. Handshake Header */
    uint8_t* hs_start = p;
    *p++ = TLS_HT_CLIENT_HELLO;
    uint8_t* hs_len_ptr = p;
    p += 3; /* Length placeholder */

    /* 3. Client Hello Body */
    *p++ = 0x03; *p++ = 0x03; /* TLS 1.2 */
    
    /* Random (32 bytes) */
    uint32_t time = (uint32_t)hpet_time_ms();
    memcpy(p, &time, 4); p += 4;
    
    /* Use PRNG for the rest of the random bytes */
    for (int i = 0; i < 7; i++) {
        *(uint32_t*)p = prng_next(); p += 4;
    }

    /* Session ID (0) */
    *p++ = 0;

    /* Cipher Suites */
    /* Advertise TLS_RSA_WITH_AES_128_CBC_SHA (0x002F) and others */
    uint16_t ciphers[] = { 0x002F, 0x0035, 0xC013 }; 
    uint16_t cipher_len = sizeof(ciphers);
    
    *p++ = (cipher_len >> 8) & 0xFF;
    *p++ = cipher_len & 0xFF;
    for (size_t i=0; i<sizeof(ciphers)/2; i++) {
        *p++ = (ciphers[i] >> 8) & 0xFF;
        *p++ = ciphers[i] & 0xFF;
    }

    /* Compression Methods (0 = null) */
    *p++ = 1;
    *p++ = 0;

    /* Extensions Length Placeholder */
    uint8_t* ext_len_ptr = p;
    p += 2;
    uint8_t* ext_start = p;

    /* Extension: Server Name (SNI) */
    if (hostname && *hostname) {
        size_t host_len = strlen(hostname);
        
        *p++ = 0x00; *p++ = 0x00; /* Type: server_name */
        
        /* Extension Length */
        size_t sni_ext_len = 2 + 1 + 2 + host_len; /* ListLen(2) + Type(1) + NameLen(2) + Name */
        *p++ = (sni_ext_len >> 8) & 0xFF;
        *p++ = sni_ext_len & 0xFF;
        
        /* Server Name List Length */
        size_t sni_list_len = 1 + 2 + host_len;
        *p++ = (sni_list_len >> 8) & 0xFF;
        *p++ = sni_list_len & 0xFF;
        
        *p++ = 0x00; /* Name Type: host_name */
        *p++ = (host_len >> 8) & 0xFF;
        *p++ = host_len & 0xFF;
        memcpy(p, hostname, host_len);
        p += host_len;
    }

    /* Fill Extensions Length */
    size_t ext_total_len = p - ext_start;
    ext_len_ptr[0] = (ext_total_len >> 8) & 0xFF;
    ext_len_ptr[1] = ext_total_len & 0xFF;

    /* Fill Lengths */
    size_t hs_len = p - hs_start - 4; /* Exclude Type(1) + Len(3) */
    hs_len_ptr[0] = (hs_len >> 16) & 0xFF;
    hs_len_ptr[1] = (hs_len >> 8) & 0xFF;
    hs_len_ptr[2] = hs_len & 0xFF;

    size_t rec_len = p - buf - 5; /* Exclude Type(1) + Ver(2) + Len(2) */
    len_ptr[0] = (rec_len >> 8) & 0xFF;
    len_ptr[1] = rec_len & 0xFF;

    return (p - buf);
}

int tls_handle_rx(tls_context_t* ctx, const uint8_t* data, size_t len) {
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    while (p + 5 <= end) {
        uint8_t type = p[0];
        uint16_t length = (p[3] << 8) | p[4];
        
        p += 5;
        if (p + length > end) return 0; /* Fragmented packet? */

        if (type == TLS_RT_HANDSHAKE) {
            uint8_t hs_type = p[0];
            serial("[TLS] Handshake msg type: %d len: %d\n", hs_type, length);
            
            if (hs_type == TLS_HT_SERVER_HELLO) {
                serial("[TLS] Server Hello received.\n");
            } else if (hs_type == TLS_HT_CERTIFICATE) {
                serial("[TLS] Certificate received.\n");
            } else if (hs_type == TLS_HT_SERVER_HELLO_DONE) {
                serial("[TLS] Server Hello Done.\n");
                serial("[TLS] ERROR: Crypto stack (RSA/ECC) missing to proceed with Key Exchange.\n");
                ctx->state = TLS_STATE_ERROR;
                return -1;
            }
        } else if (type == TLS_RT_ALERT) {
            serial("[TLS] Alert: Level=%d Desc=%d\n", p[0], p[1]);
            ctx->state = TLS_STATE_ERROR;
            return -1;
        }

        p += length;
    }
    return 0;
}
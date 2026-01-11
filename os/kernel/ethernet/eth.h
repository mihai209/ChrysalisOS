#pragma once
#include <stdint.h>
#include "net_device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806

typedef struct {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

/* Endianness Helpers */
static inline uint16_t htons(uint16_t v) { return (v << 8) | (v >> 8); }
static inline uint16_t ntohs(uint16_t v) { return (v << 8) | (v >> 8); }
static inline uint32_t htonl(uint32_t v) { 
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | 
           ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF); 
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

void eth_send(net_device_t* dev, const uint8_t* dst, uint16_t type, const void* data, size_t len);
void eth_handle_packet(net_device_t* dev, const void* data, size_t len);

#ifdef __cplusplus
}
#endif
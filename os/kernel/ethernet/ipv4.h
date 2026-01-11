#pragma once
#include "eth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP 17

typedef struct {
    uint8_t  ihl : 4;
    uint8_t  version : 4;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed)) ipv4_header_t;

void ipv4_handle_packet(net_device_t* dev, const void* data, size_t len);
int ipv4_send(net_device_t* dev, uint32_t dst_ip, uint8_t proto, const void* data, size_t len);

typedef void (*icmp_callback_t)(uint32_t src_ip, const uint8_t* data, size_t len);
void ipv4_set_icmp_callback(icmp_callback_t callback);

#ifdef __cplusplus
}
#endif
#pragma once
#include "eth.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed)) arp_packet_t;

void arp_send_request(net_device_t* dev, uint32_t ip);
void arp_handle_packet(net_device_t* dev, const void* data, size_t len);
int arp_lookup(uint32_t ip, uint8_t* mac_out);
void arp_print_cache(void);

#ifdef __cplusplus
}
#endif
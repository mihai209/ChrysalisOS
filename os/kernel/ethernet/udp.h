#pragma once
#include "ipv4.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

void udp_handle_packet(net_device_t* dev, uint32_t src_ip, const void* data, size_t len);
int udp_send(net_device_t* dev, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, size_t len);

typedef void (*udp_callback_t)(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len);
void udp_set_callback(udp_callback_t callback);

#ifdef __cplusplus
}
#endif
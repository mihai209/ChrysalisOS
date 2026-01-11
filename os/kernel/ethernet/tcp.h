#pragma once
#include "ipv4.h"
#include "net_device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

void tcp_handle_packet(net_device_t* dev, uint32_t src_ip, const void* data, size_t len);
int tcp_send_packet(net_device_t* dev, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, 
                    uint32_t seq, uint32_t ack, uint8_t flags, 
                    const void* data, size_t len);

typedef void (*tcp_callback_t)(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, uint8_t flags, uint32_t seq, uint32_t ack, const uint8_t* data, size_t len);
void tcp_set_callback(tcp_callback_t callback);

#ifdef __cplusplus
}
#endif
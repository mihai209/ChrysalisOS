#include "udp.h"
#include "../mm/kmalloc.h"
#include "../string.h"

extern void serial(const char *fmt, ...);

static udp_callback_t udp_cb = 0;

void udp_set_callback(udp_callback_t callback) {
    udp_cb = callback;
}

void udp_handle_packet(net_device_t* dev, uint32_t src_ip, const void* data, size_t len) {
    (void)dev;
    if (len < sizeof(udp_header_t)) return;
    
    udp_header_t* hdr = (udp_header_t*)data;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t data_len = ntohs(hdr->len) - sizeof(udp_header_t);
    
    serial("[UDP] Recv from %d.%d.%d.%d:%d to port %d len %d\n",
           src_ip & 0xFF, (src_ip>>8)&0xFF, (src_ip>>16)&0xFF, (src_ip>>24)&0xFF,
           src_port, dst_port, data_len);

    if (udp_cb) {
        udp_cb(src_ip, src_port, (const uint8_t*)data + sizeof(udp_header_t), data_len);
    }
}

int udp_send(net_device_t* dev, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, size_t len) {
    size_t total_len = sizeof(udp_header_t) + len;
    uint8_t* packet = (uint8_t*)kmalloc(total_len);
    if (!packet) return -1;

    udp_header_t* hdr = (udp_header_t*)packet;
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->len = htons(total_len);
    hdr->checksum = 0; /* Optional in IPv4 */

    memcpy(packet + sizeof(udp_header_t), data, len);

    int ret = ipv4_send(dev, dst_ip, IP_PROTO_UDP, packet, total_len);
    kfree(packet);
    
    if (ret == 0) serial("[UDP] Sent %d bytes to %x:%d\n", len, dst_ip, dst_port);
    return ret;
}
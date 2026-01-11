#include "eth.h"
#include "arp.h"
#include "ipv4.h"
#include "../mm/kmalloc.h"
#include "../string.h"

extern void serial(const char *fmt, ...);

void eth_send(net_device_t* dev, const uint8_t* dst, uint16_t type, const void* data, size_t len) {
    if (!dev) return;

    size_t total_len = sizeof(eth_header_t) + len;
    if (total_len < 60) total_len = 60; /* Min Ethernet frame size */

    uint8_t* frame = (uint8_t*)kmalloc(total_len);
    if (!frame) return;

    memset(frame, 0, total_len);
    eth_header_t* hdr = (eth_header_t*)frame;

    memcpy(hdr->dst, dst, 6);
    memcpy(hdr->src, dev->mac, 6);
    hdr->type = htons(type);

    memcpy(frame + sizeof(eth_header_t), data, len);

    dev->send(dev, frame, total_len);
    kfree(frame);
}

void eth_handle_packet(net_device_t* dev, const void* data, size_t len) {
    if (len < sizeof(eth_header_t)) return;

    eth_header_t* hdr = (eth_header_t*)data;
    uint16_t type = ntohs(hdr->type);
    
    /* serial("[ETH] Frame dst=%02x:%02x:%02x:%02x:%02x:%02x type=%04x\n",
           hdr->dst[0], hdr->dst[1], hdr->dst[2], 
           hdr->dst[3], hdr->dst[4], hdr->dst[5], type); */

    uint8_t* payload = (uint8_t*)data + sizeof(eth_header_t);
    size_t payload_len = len - sizeof(eth_header_t);

    if (type == ETH_TYPE_ARP) {
        arp_handle_packet(dev, payload, payload_len);
    } else if (type == ETH_TYPE_IP) {
        ipv4_handle_packet(dev, payload, payload_len);
    }
}
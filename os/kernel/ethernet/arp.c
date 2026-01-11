#include "arp.h"
#include "../string.h"
#include "../terminal.h"

extern void serial(const char *fmt, ...);

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2
#define ARP_HW_ETH     1

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    int valid;
} arp_entry_t;

static arp_entry_t arp_cache[8];
static int arp_victim_idx = 0;

void arp_handle_packet(net_device_t* dev, const void* data, size_t len) {
    if (len < sizeof(arp_packet_t)) return;
    
    arp_packet_t* pkt = (arp_packet_t*)data;
    uint16_t op = ntohs(pkt->opcode);
    uint32_t src_ip = pkt->src_ip; // Already Network Byte Order

    /* Update Cache */
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == src_ip) {
            slot = i; break;
        }
        if (!arp_cache[i].valid) slot = i;
    }
    
    /* If cache full, evict round-robin */
    if (slot == -1) {
        slot = arp_victim_idx;
        arp_victim_idx = (arp_victim_idx + 1) % 8;
    }

    if (slot != -1) {
        arp_cache[slot].ip = src_ip;
        memcpy(arp_cache[slot].mac, pkt->src_mac, 6);
        arp_cache[slot].valid = 1;
    }

    if (op == ARP_OP_REQUEST) {
        /* Check if it's for us */
        if (pkt->dst_ip == dev->ip) {
            serial("[ARP] Request for me from %d.%d.%d.%d\n", 
                src_ip & 0xFF, (src_ip>>8)&0xFF, (src_ip>>16)&0xFF, (src_ip>>24)&0xFF);
            
            arp_packet_t reply;
            reply.hw_type = htons(ARP_HW_ETH);
            reply.proto_type = htons(ETH_TYPE_IP);
            reply.hw_len = 6;
            reply.proto_len = 4;
            reply.opcode = htons(ARP_OP_REPLY);
            
            memcpy(reply.src_mac, dev->mac, 6);
            reply.src_ip = dev->ip;
            memcpy(reply.dst_mac, pkt->src_mac, 6);
            reply.dst_ip = pkt->src_ip;
            
            eth_send(dev, pkt->src_mac, ETH_TYPE_ARP, &reply, sizeof(reply));
        }
    } else if (op == ARP_OP_REPLY) {
        serial("[ARP] Reply from %d.%d.%d.%d is at %02x:%02x:%02x:%02x:%02x:%02x\n",
               src_ip & 0xFF, (src_ip>>8)&0xFF, (src_ip>>16)&0xFF, (src_ip>>24)&0xFF,
               pkt->src_mac[0], pkt->src_mac[1], pkt->src_mac[2],
               pkt->src_mac[3], pkt->src_mac[4], pkt->src_mac[5]);
    }
}

void arp_send_request(net_device_t* dev, uint32_t ip) {
    arp_packet_t req;
    req.hw_type = htons(ARP_HW_ETH);
    req.proto_type = htons(ETH_TYPE_IP);
    req.hw_len = 6;
    req.proto_len = 4;
    req.opcode = htons(ARP_OP_REQUEST);
    
    memcpy(req.src_mac, dev->mac, 6);
    req.src_ip = dev->ip;
    memset(req.dst_mac, 0xFF, 6); /* Broadcast */
    req.dst_ip = ip;
    
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    eth_send(dev, broadcast, ETH_TYPE_ARP, &req, sizeof(req));
}

int arp_lookup(uint32_t ip, uint8_t* mac_out) {
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac_out, arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

void arp_print_cache(void) {
    terminal_writestring("ARP Cache:\n");
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid) {
            uint32_t ip = arp_cache[i].ip;
            uint8_t* m = arp_cache[i].mac;
            terminal_printf("  %d.%d.%d.%d  ->  %02x:%02x:%02x:%02x:%02x:%02x\n",
                ip&0xFF, (ip>>8)&0xFF, (ip>>16)&0xFF, (ip>>24)&0xFF,
                m[0], m[1], m[2], m[3], m[4], m[5]);
        }
    }
}
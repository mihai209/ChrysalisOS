#include "net.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../ethernet/net_device.h"
#include "../ethernet/ipv4.h"
#include "../ethernet/udp.h"
#include "../ethernet/arp.h"
#include "../ethernet/eth.h" /* for htons/htonl */
#include "../ethernet/dns.h"
#include "../ethernet/dhcp.h"
#include "../ethernet/net.h" /* for net_poll */

extern "C" void serial(const char *fmt, ...);
extern "C" uint64_t hpet_time_ms(void);
extern "C" int atoi(const char* str);

/* Helper: Parse IP string "192.168.1.1" to Network Byte Order uint32 */
static uint32_t parse_ip(const char* s) {
    uint8_t bytes[4] = {0,0,0,0};
    int idx = 0;
    int val = 0;
    
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
        } else if (*s == '.') {
            bytes[idx++] = (uint8_t)val;
            val = 0;
            if (idx >= 4) break;
        }
        s++;
    }
    if (idx < 4) bytes[idx] = (uint8_t)val;

    /* Construct uint32 (Little Endian machine -> Memory: bytes[0]...bytes[3]) */
    /* We want Memory: 192 168 1 1 */
    /* On LE, 0xAABBCCDD is stored as DD CC BB AA */
    /* So we want 0x0101A8C0 */
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

/* ICMP Header for Ping */
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

static uint16_t checksum(void* vdata, size_t length) {
    char* data = (char*)vdata;
    uint32_t acc = 0;
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word;
        memcpy(&word, data + i, 2);
        acc += ntohs(word);
    }
    if (length & 1) {
        uint16_t word = 0;
        memcpy(&word, data + length - 1, 1);
        acc += ntohs(word);
    }
    while (acc >> 16) acc = (acc & 0xFFFF) + (acc >> 16);
    return htons(~acc);
}

static void cmd_usage() {
    terminal_writestring("Usage: net <command> [args]\n");
    terminal_writestring("Commands:\n");
    terminal_writestring("  info            Show network device info\n");
    terminal_writestring("  arp             Show ARP cache\n");
    terminal_writestring("  ping <ip> [--timeout sec]  Send ICMP Echo Request\n");
    terminal_writestring("  dhcp            Auto-configure via DHCP\n");
    terminal_writestring("  udp <ip> <port> <msg>  Send UDP packet\n");
}

static volatile bool ping_reply_received = false;
static uint32_t ping_reply_ip = 0;
static uint16_t ping_reply_seq = 0;

static void ping_callback(uint32_t src_ip, const uint8_t* data, size_t len) {
    (void)len;
    /* Check if it is Echo Reply (Type 0) */
    if (data[0] == 0) {
        ping_reply_received = true;
        ping_reply_ip = src_ip;
        /* Extract sequence number (offset 6 in ICMP header) */
        /* Header: Type(1), Code(1), Csum(2), ID(2), Seq(2) */
        if (len >= 8) {
            /* Data is raw bytes, construct uint16 */
            uint16_t raw_seq = (data[6] << 8) | data[7];
            ping_reply_seq = (raw_seq >> 8) | (raw_seq << 8); /* ntohs */
        }
    }
}

extern "C" int cmd_net(int argc, char** argv) {
    if (argc < 2) {
        cmd_usage();
        return -1;
    }

    net_device_t* dev = net_get_primary_device();
    if (!dev) {
        terminal_writestring("Error: No network device initialized.\n");
        return -1;
    }

    const char* sub = argv[1];

    if (strcmp(sub, "info") == 0) {
        terminal_printf("Device: %s\n", dev->name);
        terminal_printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            dev->mac[0], dev->mac[1], dev->mac[2], dev->mac[3], dev->mac[4], dev->mac[5]);
        
        uint32_t ip = dev->ip;
        terminal_printf("IP: %d.%d.%d.%d\n", 
            ip&0xFF, (ip>>8)&0xFF, (ip>>16)&0xFF, (ip>>24)&0xFF);
            
        uint32_t gw = dev->gateway;
        terminal_printf("GW: %d.%d.%d.%d\n", 
            gw&0xFF, (gw>>8)&0xFF, (gw>>16)&0xFF, (gw>>24)&0xFF);
            
        uint32_t dns = dev->dns_server;
        terminal_printf("DNS: %d.%d.%d.%d\n", 
            dns&0xFF, (dns>>8)&0xFF, (dns>>16)&0xFF, (dns>>24)&0xFF);
        return 0;
    }

    if (strcmp(sub, "arp") == 0) {
        arp_print_cache();
        return 0;
    }

    if (strcmp(sub, "dhcp") == 0) {
        dhcp_discover();
        return 0; /* Status printed by dhcp_discover */
    }

    if (strcmp(sub, "ping") == 0) {
        if (argc < 3) {
            terminal_writestring("Usage: net ping <ip>\n");
            return -1;
        }
        uint32_t target_ip = parse_ip(argv[2]);
        
        /* If parse_ip returned 0 (invalid or 0.0.0.0), try DNS */
        if (target_ip == 0) {
            target_ip = dns_resolve(argv[2]);
            if (target_ip == 0) {
                terminal_writestring("Could not resolve host.\n");
                return -1;
            }
        }

        uint32_t timeout_sec = 5;

        /* Parse options */
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
                timeout_sec = atoi(argv[i+1]);
                if (timeout_sec > 60) timeout_sec = 60;
                if (timeout_sec < 1) timeout_sec = 1;
                i++;
            }
        }
        
        terminal_printf("Pinging %d.%d.%d.%d...\n", 
            target_ip&0xFF, (target_ip>>8)&0xFF, (target_ip>>16)&0xFF, (target_ip>>24)&0xFF);

        /* Construct ICMP Echo Request */
        size_t payload_size = 32;
        size_t total_size = sizeof(icmp_header_t) + payload_size;
        uint8_t* pkt = (uint8_t*)kmalloc(total_size);
        if (!pkt) return -1;

        memset(pkt, 0, total_size);
        icmp_header_t* icmp = (icmp_header_t*)pkt;
        icmp->type = 8; /* Echo Request */
        icmp->code = 0;
        icmp->id = htons(1);
        icmp->seq = htons(1);
        
        /* Fill payload */
        for (size_t i=0; i<payload_size; i++) pkt[sizeof(icmp_header_t)+i] = (uint8_t)i;

        icmp->checksum = checksum(pkt, total_size);

        /* Setup Callback */
        ping_reply_received = false;
        ipv4_set_icmp_callback(ping_callback);

        /* Send Loop (Retry if ARP miss) */
        uint64_t start_time = hpet_time_ms();
        uint64_t timeout_ms = timeout_sec * 1000;
        bool sent = false;

        while ((hpet_time_ms() - start_time) < timeout_ms) {
            if (!sent) {
                int res = ipv4_send(dev, target_ip, IP_PROTO_ICMP, pkt, total_size);
                if (res == 0) {
                    sent = true;
                } else {
                    /* ARP miss likely, wait a bit and retry */
                    /* Don't spam send, wait 100ms */
                    uint64_t wait_start = hpet_time_ms();
                    while ((hpet_time_ms() - wait_start) < 100) {
                        net_poll();
                        asm volatile("pause");
                    }
                    continue; 
                }
            }

            net_poll();
            
            if (ping_reply_received) {
                uint64_t rtt = hpet_time_ms() - start_time;
                terminal_printf("64 bytes from %d.%d.%d.%d: icmp_seq=%d ttl=64 time=%d ms\n",
                    ping_reply_ip&0xFF, (ping_reply_ip>>8)&0xFF, (ping_reply_ip>>16)&0xFF, (ping_reply_ip>>24)&0xFF,
                    ping_reply_seq, (uint32_t)rtt);
                break;
            }
            asm volatile("pause");
        }

        if (!ping_reply_received) {
            terminal_writestring("Request timed out.\n");
        }

        ipv4_set_icmp_callback(NULL);
        kfree(pkt);
        return 0;
    }

    if (strcmp(sub, "udp") == 0) {
        if (argc < 5) {
            terminal_writestring("Usage: net udp <ip> <port> <msg>\n");
            return -1;
        }
        uint32_t target_ip = parse_ip(argv[2]);
        uint16_t port = atoi(argv[3]);
        const char* msg = argv[4];

        terminal_printf("Sending UDP to %s:%d...\n", argv[2], port);
        udp_send(dev, target_ip, 12345, port, msg, strlen(msg));
        return 0;
    }

    cmd_usage();
    return -1;
}
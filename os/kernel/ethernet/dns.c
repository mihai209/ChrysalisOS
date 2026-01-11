#include "dns.h"
#include "udp.h"
#include "net_device.h"
#include "net.h"
#include "../mm/kmalloc.h"
#include "../string.h"
#include "../terminal.h"

extern void serial(const char *fmt, ...);
extern uint64_t hpet_time_ms(void);

static volatile bool dns_resolved = false;
static uint32_t dns_result_ip = 0;

static void dns_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len) {
    (void)src_ip; (void)src_port;
    if (len < 12) return;
    
    /* Skip Header */
    const uint8_t* p = data + 12;
    const uint8_t* end = data + len;
    
    /* Skip Question Section */
    while (p < end && *p != 0) {
        p += (*p) + 1;
    }
    p++; /* Skip null byte */
    p += 4; /* Skip QTYPE, QCLASS */
    
    /* Parse Answers */
    uint16_t ancount = (data[6] << 8) | data[7];
    
    for (int i = 0; i < ancount; i++) {
        if (p >= end) break;
        
        /* Skip Name (handle compression) */
        if ((*p & 0xC0) == 0xC0) {
            p += 2;
        } else {
            while (p < end && *p != 0) p += (*p) + 1;
            p++;
        }
        
        if (p + 10 > end) break;
        
        uint16_t type = (p[0] << 8) | p[1];
        uint16_t class = (p[2] << 8) | p[3];
        uint16_t rdlen = (p[8] << 8) | p[9];
        p += 10;
        
        if (type == 1 && class == 1 && rdlen == 4) { /* Type A, Class IN */
            if (p + 4 <= end) {
                dns_result_ip = *(uint32_t*)p;
                dns_resolved = true;
                return;
            }
        }
        p += rdlen;
    }
}

uint32_t dns_resolve(const char* domain) {
    net_device_t* dev = net_get_primary_device();
    if (!dev) return 0;

    terminal_printf("Resolving %s...\n", domain);

    size_t len = 12 + (strlen(domain) + 2) + 4;
    uint8_t* pkt = (uint8_t*)kmalloc(len);
    if (!pkt) return 0;
    memset(pkt, 0, len);
    
    /* Header */
    pkt[1] = 0xAB; /* ID */
    pkt[2] = 0x01; /* Recursion Desired */
    pkt[5] = 1;    /* QDCOUNT = 1 */
    
    /* Question */
    uint8_t* q = pkt + 12;
    const char* s = domain;
    while (*s) {
        const char* next = strchr(s, '.');
        int label_len = next ? (next - s) : strlen(s);
        *q++ = label_len;
        memcpy(q, s, label_len);
        q += label_len;
        if (next) s = next + 1;
        else break;
    }
    *q++ = 0;
    
    *q++ = 0; *q++ = 1; /* QTYPE A */
    *q++ = 0; *q++ = 1; /* QCLASS IN */
    
    dns_resolved = false;
    udp_set_callback(dns_callback);
    
    /* Use configured DNS or fallback to Google (8.8.8.8) */
    uint32_t dns_server = dev->dns_server;
    if (dns_server == 0) {
        dns_server = 0x08080808; /* 8.8.8.8 */
        terminal_writestring("DNS: Using fallback 8.8.8.8\n");
    }
    
    /* Retry loop for ARP */
    bool sent = false;
    uint64_t start = hpet_time_ms();
    while (hpet_time_ms() - start < 2000) {
        if (udp_send(dev, dns_server, 55555, 53, pkt, q - pkt) == 0) {
            sent = true;
            break;
        }
        net_poll(); /* Process ARP */
    }
    kfree(pkt);
    
    if (!sent) {
        terminal_writestring("DNS: Failed to send query (ARP timeout?)\n");
        udp_set_callback(NULL);
        return 0;
    }
    
    /* Wait for response */
    start = hpet_time_ms();
    while (!dns_resolved && (hpet_time_ms() - start < 3000)) {
        net_poll();
    }
    
    udp_set_callback(NULL);
    return dns_resolved ? dns_result_ip : 0;
}
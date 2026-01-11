#include "dhcp.h"
#include "udp.h"
#include "net_device.h"
#include "net.h"
#include "../mm/kmalloc.h"
#include "../string.h"
#include "../terminal.h"
#include "eth.h"

extern void serial(const char *fmt, ...);
extern uint32_t timer_get_ticks(void);

/* DHCP constants */
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5
#define DHCPNAK      6

#define OPT_SUBNET_MASK 1
#define OPT_ROUTER      3
#define OPT_DNS         6
#define OPT_REQ_IP      50
#define OPT_MSG_TYPE    53
#define OPT_SERVER_ID   54
#define OPT_END         255

/* DHCP states */
static volatile int dhcp_state = 0;
/*
 0 = idle
 1 = DISCOVER sent, waiting OFFER
 2 = OFFER received, sending REQUEST, waiting ACK
 3 = DONE (ACK received)
*/

static uint32_t dhcp_xid = 0;
static uint32_t offered_ip = 0;
static uint32_t server_ip = 0;

/* -------------------------------------------------- */
/* helpers (no libc) */

static inline uint16_t ntohs_u16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

static inline uint32_t ntohl_u32(uint32_t v) {
    return ((v & 0xFF000000) >> 24) |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x000000FF) << 24);
}

static inline uint32_t htonl_u32(uint32_t v) {
    return ntohl_u32(v);
}

/* -------------------------------------------------- */

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[0];
} __attribute__((packed)) dhcp_packet_t;

/* -------------------------------------------------- */

static void dhcp_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len);

static void send_dhcp_packet(net_device_t* dev, int type, uint32_t req_ip) {
    size_t len = sizeof(dhcp_packet_t) + 64; /* Increased buffer for options */
    uint8_t* buf = (uint8_t*)kmalloc(len);
    if (!buf) {
        serial("[DHCP] Error: OOM in send_dhcp_packet\n");
        return;
    }
    memset(buf, 0, len);

    dhcp_packet_t* pkt = (dhcp_packet_t*)buf;
    pkt->op = 1; /* BOOTREQUEST */
    pkt->htype = 1; /* Ethernet */
    pkt->hlen = 6;
    pkt->hops = 0;
    pkt->xid = htonl_u32(dhcp_xid);
    pkt->secs = 0;
    pkt->flags = htons(0x8000); /* Broadcast flag (Bit 15) - CRITIC pentru a primi ACK */
    pkt->ciaddr = 0;
    pkt->yiaddr = 0;
    pkt->siaddr = 0;
    pkt->giaddr = 0;
    memcpy(pkt->chaddr, dev->mac, 6);
    pkt->magic = htonl_u32(DHCP_MAGIC_COOKIE);

    uint8_t* opt = pkt->options;
    *opt++ = OPT_MSG_TYPE;
    *opt++ = 1;
    *opt++ = type;

    if (type == DHCPREQUEST) {
        /* Requested IP Option */
        *opt++ = OPT_REQ_IP;
        *opt++ = 4;
        memcpy(opt, &req_ip, 4);
        opt += 4;

        /* Server Identifier Option */
        *opt++ = OPT_SERVER_ID;
        *opt++ = 4;
        memcpy(opt, &server_ip, 4);
        opt += 4;
        
        serial("[DHCP] Sending REQUEST for %d.%d.%d.%d to Server %d.%d.%d.%d\n",
            req_ip&0xFF, (req_ip>>8)&0xFF, (req_ip>>16)&0xFF, (req_ip>>24)&0xFF,
            server_ip&0xFF, (server_ip>>8)&0xFF, (server_ip>>16)&0xFF, (server_ip>>24)&0xFF);
        terminal_writestring("DHCP: Sending Request...\n");
    } else {
        serial("[DHCP] Sending DISCOVER...\n");
        terminal_writestring("DHCP: Sending Discover...\n");
    }

    *opt++ = OPT_END;

    /* Send to Broadcast (255.255.255.255) */
    udp_send(dev, 0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, buf, (opt - buf));
    kfree(buf);
}

/* -------------------------------------------------- */

int dhcp_discover(void) {
    net_device_t* dev = net_get_primary_device();
    if (!dev) {
        terminal_writestring("DHCP: No network device\n");
        return -1;
    }

    terminal_writestring("DHCP: Starting negotiation...\n");

    /* Generate new XID */
    dhcp_xid = timer_get_ticks() ^ 0xDEADBEEF;
    dhcp_state = 1; /* Start in DISCOVER state */
    offered_ip = 0;
    server_ip = 0;

    udp_set_callback(dhcp_callback);
    
    int retries = 10; /* More retries */
    int last_state = 0;
    int safety_counter = 0;

    while (retries-- > 0 && dhcp_state != 3) {
        
        /* State Machine Driver */
        if (dhcp_state == 1) {
            /* State 1: Send DISCOVER */
            send_dhcp_packet(dev, DHCPDISCOVER, 0);
        } 
        else if (dhcp_state == 2) {
            /* State 2: Send REQUEST (Retransmission handled here) */
            /* Note: dhcp_callback sets state to 2 when OFFER is received */
            if (last_state != 2) {
                serial("[DHCP] Transitioned to State 2 (Requesting)\n");
            }
            send_dhcp_packet(dev, DHCPREQUEST, offered_ip);
        }

        last_state = dhcp_state;

        /* Wait loop (approx 1 second) */
        uint32_t start = timer_get_ticks();
        safety_counter = 0;
        bool state_changed = false;
        /* terminal_writestring("DHCP: Waiting for offer...\n"); */

        while ((timer_get_ticks() - start) < 100) {
            net_poll();
            asm volatile("pause");
            
            /* Fast-track: If state changed (e.g. 1->2 or 2->3), break wait immediately */
            if (dhcp_state != last_state) {
                state_changed = true;
                break;
            }

            /* Safety break to prevent infinite loop if timer is broken */
            safety_counter++;
            if (safety_counter > 10000000) break;
        }
        
        if (dhcp_state == 3) break;
        
        if (!state_changed) {
            serial("[DHCP] Timeout waiting for response (State %d). Retrying...\n", dhcp_state);
        }
    }

    udp_set_callback(NULL);

    if (dhcp_state == 3) {
        terminal_writestring("DHCP: Configuration successful!\n");
        return 0;
    }

    terminal_writestring("DHCP: Failed (Timeout or NAK)\n");
    return -1;
}

/* -------------------------------------------------- */

static void dhcp_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len) {
    /* FIX CRITIC: src_port este deja host-order din udp.c */
    if (src_port != DHCP_SERVER_PORT)
        return;

    if (len < sizeof(dhcp_packet_t))
        return;

    const dhcp_packet_t* dhcp = (const dhcp_packet_t*)data;

    if (ntohl_u32(dhcp->xid) != dhcp_xid) {
        /* serial("[DHCP] Ignored packet with wrong XID\n"); */
        return;
    }

    if (ntohl_u32(dhcp->magic) != DHCP_MAGIC_COOKIE) {
        serial("[DHCP] Ignored packet with wrong Magic Cookie\n");
        return;
    }

    uint8_t msg_type = 0;

    const uint8_t* opt = dhcp->options;
    const uint8_t* end = data + len;
    uint32_t parsed_server_ip = 0;
    uint32_t parsed_subnet = 0;
    uint32_t parsed_gateway = 0;
    uint32_t parsed_dns = 0;

    while (opt < end && *opt != 0xFF) {
        if (*opt == 0x00) {
            opt++;
            continue;
        }

        uint8_t type = opt[0];
        uint8_t olen = opt[1];

        if (opt + 2 + olen > end)
            break;

        if (type == 53 && olen == 1)
            msg_type = opt[2];
        else if (type == 54 && olen == 4)
            memcpy(&parsed_server_ip, opt + 2, 4);
        else if (type == OPT_SUBNET_MASK && olen == 4)
            memcpy(&parsed_subnet, opt + 2, 4);
        else if (type == OPT_ROUTER && olen >= 4)
            memcpy(&parsed_gateway, opt + 2, 4);
        else if (type == OPT_DNS && olen >= 4)
            memcpy(&parsed_dns, opt + 2, 4);

        opt += 2 + olen;
    }

    serial("[DHCP] Recv Message Type: %d from %x\n", msg_type, src_ip);

    if (msg_type == DHCPOFFER && dhcp_state == 1) {
        offered_ip = dhcp->yiaddr;
        /* Prefer Option 54 (Server ID), fallback to Source IP */
        server_ip = parsed_server_ip ? parsed_server_ip : src_ip;
        
        serial("[DHCP] OFFER received: IP %d.%d.%d.%d from Server %d.%d.%d.%d\n",
            offered_ip&0xFF, (offered_ip>>8)&0xFF, (offered_ip>>16)&0xFF, (offered_ip>>24)&0xFF,
            server_ip&0xFF, (server_ip>>8)&0xFF, (server_ip>>16)&0xFF, (server_ip>>24)&0xFF);
            
        terminal_writestring("DHCP: OFFER received\n");

        /* Advance state to 2. The main loop will see this and send the REQUEST. */
        dhcp_state = 2;
    }

    if (msg_type == DHCPACK && dhcp_state == 2) {
        net_device_t* dev = net_get_primary_device();
        if (dev) {
            dev->ip = dhcp->yiaddr;
            dev->subnet = parsed_subnet;
            dev->gateway = parsed_gateway ? parsed_gateway : src_ip;
            dev->dns_server = parsed_dns ? parsed_dns : src_ip;
            
            serial("[DHCP] Config: IP=%x Mask=%x GW=%x DNS=%x\n", 
                   dev->ip, dev->subnet, dev->gateway, dev->dns_server);
        }
        dhcp_state = 3;
        terminal_writestring("DHCP: ACK received\n");
        serial("[DHCP] ACK received. IP Configured.\n");
    }
    
    if (msg_type == DHCPNAK) {
        serial("[DHCP] NAK received! Retrying discovery...\n");
        dhcp_state = 1; /* Reset to start */
    }
}

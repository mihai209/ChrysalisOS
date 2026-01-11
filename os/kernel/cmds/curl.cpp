#include "curl.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../ethernet/net_device.h"
#include "../ethernet/tcp.h"
#include "../ethernet/dns.h"
#include "../ethernet/net.h"
#include "../ethernet/tls.h"
#include "../crypto/prng.h"

extern "C" void serial(const char *fmt, ...);
extern "C" uint64_t hpet_time_ms(void);
extern "C" int atoi(const char* str);

/* State Machine */
enum {
    STATE_CLOSED = 0,
    STATE_SYN_SENT,
    STATE_ESTABLISHED,
    STATE_FIN_WAIT,
    STATE_TLS_HANDSHAKE
};

static volatile int tcp_state = STATE_CLOSED;
static uint32_t my_seq = 0;
static uint32_t server_seq = 0;
static uint32_t server_ip_addr = 0;
static uint16_t local_port = 0;
static uint16_t remote_port = 0;
static bool use_tls = false;
static tls_context_t tls_ctx;
static char target_hostname[128];

/* Download Buffer */
#define DOWNLOAD_BUF_SIZE (4 * 1024 * 1024) /* 4MB limit */
static uint8_t* dl_buffer = 0;
static uint32_t dl_size = 0;
static bool dl_complete = false;

static void curl_tcp_callback(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, uint8_t flags, uint32_t seq, uint32_t ack, const uint8_t* data, size_t len) {
    if (src_ip != server_ip_addr || dst_port != local_port) return;

    if (tcp_state == STATE_SYN_SENT) {
        if ((flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK)) {
            server_seq = seq + 1;
            my_seq = ack;
            
            if (use_tls) {
                tcp_state = STATE_TLS_HANDSHAKE;
                /* Send Client Hello */
                uint8_t* buf = (uint8_t*)kmalloc(512);
                if (buf) {
                    tls_init_context(&tls_ctx, server_ip_addr, remote_port);
                    int len = tls_create_client_hello(&tls_ctx, target_hostname, buf, 512);
                    if (len > 0) {
                        net_device_t* dev = net_get_primary_device();
                        tcp_send_packet(dev, server_ip_addr, local_port, remote_port, my_seq, server_seq, TCP_FLAG_ACK | TCP_FLAG_PSH, buf, len);
                        my_seq += len;
                    }
                    kfree(buf);
                }
            } else {
                tcp_state = STATE_ESTABLISHED;
            }
        }
    }
    else if (tcp_state == STATE_TLS_HANDSHAKE) {
        if (len > 0) {
            server_seq = seq + len;
            /* Pass data to TLS engine */
            tls_handle_rx(&tls_ctx, data, len);
            /* ACK the packet */
            net_device_t* dev = net_get_primary_device();
            tcp_send_packet(dev, server_ip_addr, local_port, remote_port, my_seq, server_seq, TCP_FLAG_ACK, 0, 0);
        }
    }
    else if (tcp_state == STATE_ESTABLISHED || tcp_state == STATE_FIN_WAIT) {
        if (len > 0) {
            /* Append data */
            if (dl_size + len < DOWNLOAD_BUF_SIZE) {
                memcpy(dl_buffer + dl_size, data, len);
                dl_size += len;
            }
            
            /* Update sequence for ACK */
            server_seq = seq + len;
            
            /* Send ACK */
            net_device_t* dev = net_get_primary_device();
            if (dev) {
                tcp_send_packet(dev, server_ip_addr, local_port, remote_port, my_seq, server_seq, TCP_FLAG_ACK, 0, 0);
            }
        }

        if (flags & TCP_FLAG_FIN) {
            server_seq++; /* FIN consumes a sequence number */
            /* Send ACK for FIN */
            net_device_t* dev = net_get_primary_device();
            if (dev) {
                tcp_send_packet(dev, server_ip_addr, local_port, remote_port, my_seq, server_seq, TCP_FLAG_ACK, 0, 0);
            }
            tcp_state = STATE_CLOSED;
            dl_complete = true;
        }
    }
}

static uint32_t parse_ip_simple(const char* s) {
    uint8_t bytes[4] = {0,0,0,0};
    int idx = 0;
    int val = 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') val = val * 10 + (*s - '0');
        else if (*s == '.') { bytes[idx++] = val; val = 0; if (idx >= 4) break; }
        s++;
    }
    if (idx < 4) bytes[idx] = val;
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

extern "C" int cmd_curl(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Usage: curl <url>\n");
        return -1;
    }

    char* url = argv[1];
    char hostname[128];
    char path[256];
    int port = 80;
    use_tls = false;
    
    /* Parse URL (very basic) */
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        url += 8;
        port = 443;
        use_tls = true;
    }
    
    char* slash = strchr(url, '/');
    if (slash) {
        int host_len = slash - url;
        if (host_len > 127) host_len = 127;
        strncpy(hostname, url, host_len);
        hostname[host_len] = 0;
        strncpy(path, slash, 255);
    } else {
        strncpy(hostname, url, 127);
        strcpy(path, "/");
    }

    /* Parse Port in Hostname */
    char* colon = strchr(hostname, ':');
    if (colon) {
        *colon = 0;
        port = atoi(colon + 1);
    }

    /* Store hostname for SNI */
    strncpy(target_hostname, hostname, 127);

    /* Resolve DNS */
    uint32_t ip = 0;
    if (hostname[0] >= '0' && hostname[0] <= '9') {
        ip = parse_ip_simple(hostname);
    } else {
        ip = dns_resolve(hostname);
    }

    if (ip == 0) {
        terminal_writestring("curl: Could not resolve host.\n");
        return -1;
    }

    /* Seed PRNG for TCP/TLS randomness */
    prng_seed((uint32_t)hpet_time_ms());

    /* Setup Buffer */
    dl_buffer = (uint8_t*)kmalloc(DOWNLOAD_BUF_SIZE);
    if (!dl_buffer) {
        terminal_writestring("curl: OOM.\n");
        return -1;
    }
    dl_size = 0;
    dl_complete = false;

    /* Setup TCP */
    server_ip_addr = ip;
    local_port = 40000 + (prng_next() % 10000); /* Randomish port */
    remote_port = port;
    my_seq = prng_next();
    server_seq = 0;
    tcp_state = STATE_SYN_SENT;
    
    tcp_set_callback(curl_tcp_callback);
    
    net_device_t* dev = net_get_primary_device();
    if (!dev) {
        kfree(dl_buffer);
        return -1;
    }

    /* 1. Send SYN */
    /* Wait for connection with retry (handle ARP miss) */
    uint64_t start = hpet_time_ms();
    uint64_t last_send = start - 2000; /* Force immediate first send */

    while (tcp_state == STATE_SYN_SENT && (hpet_time_ms() - start < 5000)) {
        /* Retry SYN every 1s if no response (handles ARP drops) */
        if ((hpet_time_ms() - last_send) > 1000) {
            tcp_send_packet(dev, ip, local_port, remote_port, my_seq, 0, TCP_FLAG_SYN, 0, 0);
            last_send = hpet_time_ms();
        }
        net_poll();
        asm volatile("pause");
    }

    if (tcp_state != STATE_ESTABLISHED && tcp_state != STATE_TLS_HANDSHAKE) {
        terminal_writestring("curl: Connection timed out.\n");
        tcp_set_callback(NULL);
        kfree(dl_buffer);
        return -1;
    }

    if (use_tls) {
        terminal_writestring("curl: TLS Handshake initiated (will fail due to missing crypto)...\n");
        /* Wait a bit to receive Server Hello, then exit */
        start = hpet_time_ms();
        while (hpet_time_ms() - start < 3000) { 
            net_poll(); 
            asm volatile("pause"); 
        }
        return 0;
    }

    /* 2. Send GET Request */
    char req[512];
    int req_len = 0;
    
    strcpy(req, "GET ");
    strcat(req, path);
    strcat(req, " HTTP/1.0\r\nHost: ");
    strcat(req, hostname);
    strcat(req, "\r\nUser-Agent: ChrysalisOS-curl/0.1\r\n\r\n");
    req_len = strlen(req);

    tcp_send_packet(dev, ip, local_port, remote_port, my_seq, server_seq, TCP_FLAG_ACK | TCP_FLAG_PSH, req, req_len);
    my_seq += req_len;

    /* 3. Wait for Data */
    start = hpet_time_ms();
    while (!dl_complete && (hpet_time_ms() - start < 10000)) {
        net_poll();
        asm volatile("pause");
    }

    tcp_set_callback(NULL);

    if (dl_size == 0) {
        terminal_writestring("curl: No data received.\n");
        kfree(dl_buffer);
        return -1;
    }

    /* 4. Parse HTTP Header to find body */
    uint8_t* body = dl_buffer;
    uint32_t body_len = dl_size;
    
    for (uint32_t i = 0; i < dl_size - 4; i++) {
        if (dl_buffer[i] == '\r' && dl_buffer[i+1] == '\n' && 
            dl_buffer[i+2] == '\r' && dl_buffer[i+3] == '\n') {
            body = dl_buffer + i + 4;
            body_len = dl_size - (i + 4);
            break;
        }
    }

    /* Print body to terminal */
    for (uint32_t i = 0; i < body_len; i++) {
        terminal_putchar((char)body[i]);
    }
    if (body_len > 0 && body[body_len-1] != '\n') {
        terminal_putchar('\n');
    }

    kfree(dl_buffer);
    return 0;
}
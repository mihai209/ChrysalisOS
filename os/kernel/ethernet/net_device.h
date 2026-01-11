#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct net_device {
    char name[16];
    uint8_t mac[6];
    uint32_t ip;     /* IPv4 Address (Network Byte Order) */
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns_server;

    int (*send)(struct net_device* dev, const void* data, size_t len);
    int (*poll)(struct net_device* dev);
    
    void* priv; /* Driver private data */
} net_device_t;

void net_register_device(net_device_t* dev);
net_device_t* net_get_primary_device(void);

#ifdef __cplusplus
}
#endif
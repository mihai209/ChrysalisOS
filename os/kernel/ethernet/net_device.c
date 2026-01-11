#include "net_device.h"
#include "net.h"

/* Import serial logging */
extern void serial(const char *fmt, ...);

static net_device_t* primary_dev = NULL;

void net_register_device(net_device_t* dev) {
    if (!dev) return;
    
    serial("[NET] Registered device: %s MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->name,
           dev->mac[0], dev->mac[1], dev->mac[2],
           dev->mac[3], dev->mac[4], dev->mac[5]);

    if (!primary_dev) {
        primary_dev = dev;
        serial("[NET] Set as primary device.\n");
    }
}

net_device_t* net_get_primary_device(void) {
    return primary_dev;
}
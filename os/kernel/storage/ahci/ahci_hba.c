#include "ahci.h"
#include "../block.h"
#include "../../mem/kmalloc.h"
#include "../../string.h"

extern int ahci_port_init(int port_no, hba_port_t *port);
extern void* ahci_get_abar(void);

/* Block device wrappers */
static int ahci_block_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buf) {
    int port = (int)(uintptr_t)dev->priv;
    return ahci_read_lba(port, lba, count, buf);
}

static int ahci_block_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buf) {
    int port = (int)(uintptr_t)dev->priv;
    return ahci_write_lba(port, lba, count, buf);
}

int ahci_init(void) {
    serial("[AHCI] init start\n");

    /* 1. PCI Probe & Map */
    if (ahci_pci_probe_and_map() != 0) {
        serial("[AHCI] PCI probe failed\n");
        return -1;
    }

    /* 2. Get ABAR */
    hba_mem_t *abar = (hba_mem_t *)ahci_get_abar();
    if (!abar) return -2;

    /* 3. Check Ports Implemented (PI) */
    uint32_t pi = abar->pi;
    serial("[AHCI] Ports Implemented (PI): 0x%08x\n", pi);

    int ports_found = 0;
    for (int i = 0; i < 32; i++) {
        if (pi & (1U << i)) {
            /* Check device signature */
            hba_port_t *port = (hba_port_t *)((uint8_t *)abar + 0x100 + (i * sizeof(hba_port_t)));
            uint32_t ssts = port->ssts;
            uint32_t sig = port->sig;
            
            /* Check if device present (Det=3) and Phy ready (Ipm=1) */
            uint8_t det = ssts & 0x0F;
            if (det != 3) continue;
            if (sig == 0xEB140101) continue; /* ATAPI/SATAPI skip for now if desired, or init */

            serial("[AHCI] Port %d present (SSTS=0x%x, SIG=0x%08x)\n", i, ssts, sig);
            if (ahci_port_init(i, port) == 0) {
                ports_found++;
                
                /* Register as Block Device */
                block_device_t *bd = (block_device_t*)kmalloc(sizeof(block_device_t));
                if (bd) {
                    memset(bd, 0, sizeof(block_device_t));
                // snprintf(bd->name, 32, "ahci%d", i); // Need snprintf, manual for now:
                bd->name[0] = 'a'; bd->name[1] = 'h'; bd->name[2] = 'c'; bd->name[3] = 'i'; 
                bd->name[4] = '0' + i; bd->name[5] = 0;
                
                bd->sector_count = port_states[i].sector_count;
                bd->sector_size = 512;
                bd->read = ahci_block_read;
                bd->write = ahci_block_write;
                bd->priv = (void*)(uintptr_t)i;
                block_register(bd);
                } else {
                    serial("[AHCI] Failed to allocate block device structure\n");
                }
            }
        }
    }
    return ports_found;
}
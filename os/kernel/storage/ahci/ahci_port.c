#include "ahci.h"
#include <stdint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* forward declare command functions */
extern int ahci_send_identify(int port_no, hba_port_t *port, void *out_512);

ahci_port_state_t port_states[AHCI_MAX_PORTS];

int ahci_port_init(int port_no, hba_port_t *port) {
    serial("[AHCI] port %d: init start\n", port_no);
    port_states[port_no].port = port;
    
    /* 1. Stop Command Engine */
    port->cmd &= ~(1 << 0); /* ST = 0 */
    port->cmd &= ~(1 << 4); /* FRE = 0 */

    /* 2. Wait for CR (bit 15) and FR (bit 14) to clear */
    uint32_t timeout = get_uptime_ms() + 500;
    while ((port->cmd & (1<<15) || port->cmd & (1<<14)) && get_uptime_ms() < timeout) {
        asm volatile("pause");
    }
    if (port->cmd & ((1<<15)|(1<<14))) {
        serial("[AHCI] port %d: engine stop timeout (cmd=0x%x)\n", port_no, port->cmd);
        // Try to continue anyway, maybe it's stuck but we can rebase
    }

    /* 3. Allocate CLB (Command List Base) - 1024 bytes, 1024 align */
    void *clb = ahci_dma_alloc(1024, 1024);
    if (!clb) {
        serial("[AHCI] port %d: DMA alloc failed for CLB\n", port_no);
        return -1;
    }
    port_states[port_no].clb = clb;
    
    /* Set Physical Address in Register */
    port->clb = ahci_virt_to_phys(clb);
    port->clbu = 0;

    /* 4. Allocate FB (FIS Base) - 256 bytes, 256 align */
    void *fb = ahci_dma_alloc(256, 256);
    if (!fb) {
        serial("[AHCI] port %d: DMA alloc failed for FB\n", port_no);
        return -2;
    }
    port_states[port_no].fb = fb;
    
    /* Set Physical Address in Register */
    port->fb = ahci_virt_to_phys(fb);
    port->fbu = 0;

    /* 5. Allocate Command Tables for each slot (32 slots) */
    /* Each CT is 128-256 bytes depending on PRDT count. We use 256B, align 128 */
    for (int s = 0; s < AHCI_MAX_CMDS; ++s) {
        void *ct = ahci_dma_alloc(256, 128);
        if (!ct) {
            serial("[AHCI] port %d: DMA alloc failed for CT %d\n", port_no, s);
            return -3;
        }
        port_states[port_no].cmd_tables[s] = ct;
    }

    /* 6. Setup Port Registers */
    port->serr = 0xFFFFFFFF; /* Clear SError */
    port->is   = 0xFFFFFFFF; /* Clear Interrupt Status */
    port->ie   = 0xFFFFFFFF; /* Enable Interrupts */

    /* 7. Start Engine */
    /* Enable FRE first */
    port->cmd |= (1 << 4);
    /* Enable ST */
    port->cmd |= (1 << 0);
    
    serial("[AHCI] port %d: started (cmd=0x%08x)\n", port_no, port->cmd);

    /* run IDENTIFY to get device info */
    uint8_t ident[512];
    int r = ahci_send_identify(port_no, port, ident);
    if (r != 0) {
        serial("[AHCI] port %d: IDENTIFY failed (%d)\n", port_no, r);
    } else {
        /* parse model string at offset 54..93 (word 27..46) per ATA */
        char model[41]; /* 40 chars + NUL */
        for (int i = 0; i < 40; i++) model[i] = ident[54 + i];
        model[40] = 0;
        
        /* Swap bytes for model string (ATA strings are big-endian words) */
        for (int i = 0; i < 40; i+=2) { char tmp = model[i]; model[i] = model[i+1]; model[i+1] = tmp; }
        serial("[AHCI] port %d: identified model: %.40s\n", port_no, model);
        serial("[AHCI] port %d: LBA48 support: %s\n", port_no, (ident[167] & (1<<10)) ? "Yes" : "No");

        /* Parse Sector Count */
        uint16_t *id_words = (uint16_t*)ident;
        uint32_t lba28 = (uint32_t)id_words[60] | ((uint32_t)id_words[61] << 16);
        uint64_t lba48 = (uint64_t)id_words[100] | ((uint64_t)id_words[101] << 16) | 
                         ((uint64_t)id_words[102] << 32) | ((uint64_t)id_words[103] << 48);
        
        if (id_words[83] & (1<<10)) { /* LBA48 supported */
            port_states[port_no].sector_count = lba48;
        } else {
            port_states[port_no].sector_count = lba28;
        }
        serial("[AHCI] port %d: capacity %llu sectors\n", port_no, (unsigned long long)port_states[port_no].sector_count);
    }

    serial("[AHCI] port %d: init done\n", port_no);
    return 0;
}

/* helper to find a free command slot */
int find_cmdslot(hba_port_t *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < AHCI_MAX_CMDS; ++i) {
        if (!(slots & (1U << i))) return i;
    }
    return -1;
}

/* NOTE: the implementation of ahci_send_identify and read/write lives in ahci_cmd.c */

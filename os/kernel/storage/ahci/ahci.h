#ifndef CHR_AHCI_H
#define CHR_AHCI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------
 * General limits
 * ------------------------------------------------- */
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMDS  32

/* -------------------------------------------------
 * Kernel-provided helpers
 * (must exist in kernel)
 * ------------------------------------------------- */
extern void serial(const char *fmt, ...);

extern void *phys_to_virt(uint32_t phys_addr);

extern void *kmalloc_aligned(size_t size, size_t align);
extern void  kfree(void *ptr);

extern int register_irq_handler(
    int irq,
    void (*handler)(void *ctx),
    void *ctx
);

extern int pci_find_device_by_class(
    uint8_t class_code,
    uint8_t sub,
    uint8_t prog_if,
    uint8_t *out_bus,
    uint8_t *out_dev,
    uint8_t *out_func
);

extern uint32_t pci_read_bar32(
    uint8_t bus,
    uint8_t dev,
    uint8_t func,
    int bar_index
);

extern void pci_enable_busmaster(
    uint8_t bus,
    uint8_t dev,
    uint8_t func
);

extern uint32_t get_uptime_ms(void);

/* -------------------------------------------------
 * AHCI HBA memory structures
 * (per AHCI spec, truncated)
 * ------------------------------------------------- */

/* HBA memory base (ABAR) */
typedef volatile struct {
    uint32_t cap;     /* 0x00 */
    uint32_t ghc;     /* 0x04 */
    uint32_t is;      /* 0x08 */
    uint32_t pi;      /* 0x0C */
    uint32_t vs;      /* 0x10 */
    uint32_t ccc_ctl; /* 0x14 */
    uint32_t ccc_pts; /* 0x18 */
    uint32_t em_loc;  /* 0x1C */
    uint32_t em_ctl;  /* 0x20 */
    uint32_t cap2;    /* 0x24 */
    uint32_t bohc;    /* 0x28 */
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    /* ports start at offset 0x100 */
} hba_mem_t;

/* Per-port registers */
typedef volatile struct {
    uint32_t clb;     /* 0x00 */
    uint32_t clbu;    /* 0x04 */
    uint32_t fb;      /* 0x08 */
    uint32_t fbu;     /* 0x0C */
    uint32_t is;      /* 0x10 */
    uint32_t ie;      /* 0x14 */
    uint32_t cmd;     /* 0x18 */
    uint32_t rsv0;    /* 0x1C */
    uint32_t tfd;     /* 0x20 */
    uint32_t sig;     /* 0x24 */
    uint32_t ssts;    /* 0x28 */
    uint32_t sctl;    /* 0x2C */
    uint32_t serr;    /* 0x30 */
    uint32_t sact;    /* 0x34 */
    uint32_t ci;      /* 0x38 */
    uint32_t sntf;    /* 0x3C */
    uint32_t fbs;     /* 0x40 */
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

/* -------------------------------------------------
 * Command list / FIS structures
 * ------------------------------------------------- */

typedef struct {
    uint16_t flags;
    uint16_t prdt_len;
    uint32_t prdt_byte_count;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv[4];
} hba_cmd_header_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv;
    uint32_t dbc; /* byte count (0-based) */
} hba_prdt_entry_t;

/* FIS - Host to Device (Register FIS - type 0x27) */
typedef struct {
    uint8_t  fis_type;  // 0x27
    uint8_t  pm_port : 4;
    uint8_t  rsv0    : 3;
    uint8_t  c       : 1; // 1 = Command, 0 = Control
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} fis_reg_h2d_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt_entry[1];
} hba_cmd_tbl_t;

/* -------------------------------------------------
 * Internal per-port software state
 * ------------------------------------------------- */
typedef struct {
    hba_port_t *port;
    void *clb;
    void *fb;
    void *cmd_tables[AHCI_MAX_CMDS];
    uint64_t sector_count;
} ahci_port_state_t;

/* exported global port state table */
extern ahci_port_state_t port_states[AHCI_MAX_PORTS];

/* -------------------------------------------------
 * Internal init helpers
 * ------------------------------------------------- */
int ahci_pci_probe_and_map(void);
int find_cmdslot(hba_port_t *port);

/* DMA Allocator helpers */
void* ahci_dma_alloc(size_t size, size_t align);
uint32_t ahci_virt_to_phys(void* v);

/* -------------------------------------------------
 * Public AHCI API
 * ------------------------------------------------- */
int ahci_init(void);

int ahci_read_lba(
    int port_id,
    uint64_t lba,
    uint32_t count,
    void *buf
);

int ahci_write_lba(
    int port_id,
    uint64_t lba,
    uint32_t count,
    const void *buf
);

#ifdef __cplusplus
}
#endif

#endif /* CHR_AHCI_H */

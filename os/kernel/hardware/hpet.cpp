#include "hpet.h"
#include "../drivers/serial.h"
#include "../paging.h"
#include "acpi.h" // Expected to provide acpi_find_table

/* HPET ACPI Table Structure */
struct HPET_Table {
    struct {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oem_id[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } header;

    uint32_t event_timer_block_id;
    
    struct {
        uint8_t address_space_id;
        uint8_t register_bit_width;
        uint8_t register_bit_offset;
        uint8_t reserved;
        uint64_t address;
    } address;

    uint8_t hpet_number;
    uint16_t min_tick;
    uint8_t page_protection;
} __attribute__((packed));

/* HPET Registers (Offsets) */
#define HPET_REG_GCAP_ID    0x00 /* General Capabilities and ID */
#define HPET_REG_GEN_CONF   0x10 /* General Configuration */
#define HPET_REG_GIS        0x20 /* General Interrupt Status */
#define HPET_REG_MAIN_CNT   0xF0 /* Main Counter Value */

/* Bit definitions */
#define HPET_GCAP_COUNT_SIZE    (1 << 13) /* 1 = 64-bit, 0 = 32-bit */
#define HPET_CONF_ENABLE        (1 << 0)  /* Enable Main Counter */
#define HPET_CONF_LEGACY        (1 << 1)  /* Legacy Replacement Route */

static volatile uint8_t* hpet_base = nullptr;
static uint32_t hpet_period_fs = 0; /* Clock period in femtoseconds (10^-15 s) */
static bool hpet_is_64bit = false;

/* Helper to read/write HPET registers */
static inline uint64_t hpet_read(uint32_t reg) {
    return *(volatile uint64_t*)(hpet_base + reg);
}

static inline void hpet_write(uint32_t reg, uint64_t val) {
    *(volatile uint64_t*)(hpet_base + reg) = val;
}

void hpet_init(void) {
    serial_write_string("[HPET] Initializing...\r\n");

    /* 1. Find ACPI HPET Table */
    struct HPET_Table* table = (struct HPET_Table*)acpi_find_table("HPET");
    if (!table) {
        serial_write_string("[HPET] Table not found in ACPI. Aborting.\r\n");
        return;
    }

    uint32_t phys_addr = (uint32_t)table->address.address;
    serial_printf("[HPET] Found table at 0x%x. MMIO Phys: 0x%x\n", (uint32_t)table, phys_addr);

    /* 2. Map HPET MMIO region */
    /* We map it identity (virt=phys) for simplicity in freestanding kernel.
       Using PAGE_PRESENT | PAGE_RW | PAGE_PCD (0x10) for uncached MMIO. */
    uint32_t flags = PAGE_PRESENT | PAGE_RW | 0x10; 
    if (paging_map_page(phys_addr, phys_addr, flags) != 0) {
        serial_write_string("[HPET] Failed to map MMIO page.\r\n");
        return;
    }
    hpet_base = (volatile uint8_t*)phys_addr;
    serial_write_string("[HPET] MMIO mapped successfully.\r\n");

    /* 3. Read Capabilities */
    uint64_t gcap = hpet_read(HPET_REG_GCAP_ID);
    hpet_period_fs = (uint32_t)(gcap >> 32);
    hpet_is_64bit = (gcap & HPET_GCAP_COUNT_SIZE) != 0;
    
    uint32_t vendor_id = (uint32_t)(gcap >> 16) & 0xFFFF;
    uint32_t rev_id = (uint32_t)gcap & 0xFF;

    serial_printf("[HPET] Vendor: 0x%x, Rev: 0x%x, 64-bit: %s\n", 
                  vendor_id, rev_id, hpet_is_64bit ? "Yes" : "No");
    
    if (hpet_period_fs == 0) {
        serial_write_string("[HPET] Error: Invalid period (0). Aborting.\r\n");
        hpet_base = nullptr;
        return;
    }

    /* Calculate frequency in MHz for display: 10^15 / period / 10^6 = 10^9 / period */
    uint32_t freq_mhz = 1000000000 / hpet_period_fs; // Approximate
    serial_printf("[HPET] Period: %u fs (~%u MHz)\n", hpet_period_fs, freq_mhz);

    /* 4. Enable Main Counter */
    uint64_t conf = hpet_read(HPET_REG_GEN_CONF);
    serial_printf("[HPET] Old Config: 0x%x\n", (uint32_t)conf);

    /* We DO NOT set LEGACY_REPLACEMENT (bit 1) to keep PIT working */
    conf |= HPET_CONF_ENABLE;
    conf &= ~HPET_CONF_LEGACY; /* Ensure legacy replacement is OFF */
    
    hpet_write(HPET_REG_GEN_CONF, conf);
    
    uint64_t new_conf = hpet_read(HPET_REG_GEN_CONF);
    serial_printf("[HPET] New Config: 0x%x (Enabled)\n", (uint32_t)new_conf);

    /* 5. Verify Counter */
    uint64_t t1 = hpet_get_ticks();
    for(volatile int i=0; i<10000; i++); /* short delay */
    uint64_t t2 = hpet_get_ticks();

    if (t2 > t1) {
        serial_printf("[HPET] Counter working: %u -> %u\n", (uint32_t)t1, (uint32_t)t2);
    } else {
        serial_write_string("[HPET] Warning: Counter not incrementing!\r\n");
    }

    /* 6. Final Verification */
    serial_write_string("[HPET] Running final verification...\r\n");
    
    uint64_t start_time = hpet_time_ms();
    serial_printf("[HPET] Current time: %u ms. Waiting 10ms...\n", (uint32_t)start_time);
    
    hpet_delay_ms(10);
    
    uint64_t end_time = hpet_time_ms();
    uint32_t delta = (uint32_t)(end_time - start_time);
    serial_printf("[HPET] End time: %u ms. Delta: %u ms\n", (uint32_t)end_time, delta);
    
    if (delta >= 10 && end_time > start_time) {
        serial_write_string("[HPET] Monotonic check PASSED. Implementation DONE.\r\n");
    } else {
        serial_write_string("[HPET] Monotonic check FAILED (delta too small or time backwards).\r\n");
    }
}

bool hpet_is_active(void) {
    return hpet_base != nullptr;
}

uint64_t hpet_get_ticks(void) {
    if (!hpet_base) return 0;

    /* On 32-bit systems, reading a 64-bit register might not be atomic.
       We read high-low-high to ensure consistency. */
    if (hpet_is_64bit) {
        volatile uint32_t* regs = (volatile uint32_t*)(hpet_base + HPET_REG_MAIN_CNT);
        uint32_t high1, low, high2;
        do {
            high1 = regs[1]; // Offset 4
            low = regs[0];   // Offset 0
            high2 = regs[1];
        } while (high1 != high2);
        return ((uint64_t)high1 << 32) | low;
    } else {
        /* 32-bit counter */
        return hpet_read(HPET_REG_MAIN_CNT);
    }
}

/* 
 * Convert ticks to nanoseconds.
 * Formula: ns = ticks * period_fs / 1,000,000
 * 
 * To avoid 64-bit overflow with large tick counts (uptime > 5 hours),
 * we split the math:
 * period_fs = Q * 1,000,000 + R
 * ns = ticks * Q + (ticks * R) / 1,000,000
 * 
 * Note: This extends safe range to ~20-50 days of uptime depending on frequency.
 */
uint64_t hpet_time_ns(void) {
    if (!hpet_base) return 0;

    uint64_t ticks = hpet_get_ticks();
    uint64_t p_ns = hpet_period_fs / 1000000;      // Integer part (ns)
    uint64_t p_rem = hpet_period_fs % 1000000;     // Remainder (fs)

    uint64_t ns = ticks * p_ns;
    ns += (ticks * p_rem) / 1000000;
    
    return ns;
}

uint64_t hpet_time_us(void) {
    return hpet_time_ns() / 1000;
}

uint64_t hpet_time_ms(void) {
    return hpet_time_ns() / 1000000;
}

void hpet_delay_us(uint32_t us) {
    if (!hpet_base) return;

    /* Calculate target ticks.
       Time = ticks * period_fs
       ticks = Time / period_fs
       Time = us * 10^9 fs (since 1 us = 10^-6 s = 10^9 fs)
       ticks = (us * 10^9) / period_fs
    */
    
    uint64_t start = hpet_get_ticks();
    uint64_t ticks_needed = ((uint64_t)us * 1000000000ULL) / hpet_period_fs;

    /* Busy wait */
    while (hpet_get_ticks() - start < ticks_needed) {
        asm volatile("pause");
    }
}

void hpet_delay_ms(uint32_t ms) {
    hpet_delay_us(ms * 1000);
}

/* GCC 32-bit helper for 64-bit division (fixes undefined reference to __udivdi3) */
extern "C" uint64_t __udivdi3(uint64_t n, uint64_t d) {
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r <<= 1;
        if ((n >> i) & 1) r |= 1;
        if (r >= d) {
            r -= d;
            q |= (1ULL << i);
        }
    }
    return q;
}

/* GCC 32-bit helper for 64-bit modulo */
extern "C" uint64_t __umoddi3(uint64_t n, uint64_t d) {
    uint64_t q = __udivdi3(n, d);
    return n - q * d;
}
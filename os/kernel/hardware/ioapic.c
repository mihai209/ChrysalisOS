#include "ioapic.h"
#include "../paging.h"
#include "../drivers/serial.h"

// IOAPIC Registers
#define IOAPICID  0x00
#define IOAPICVER 0x01
#define IOAPICARB 0x02
#define IOREDTBL  0x10

// Default virtual address for IOAPIC (mapped to 0xFEC00000)
static volatile uint32_t* ioapic_base = (volatile uint32_t*)0xFEC00000;
static uint32_t ioapic_gsi_base = 0;

static uint32_t ioapic_read(uint32_t reg) {
    *(volatile uint32_t*)(ioapic_base) = reg;
    return *(volatile uint32_t*)((uint8_t*)ioapic_base + 0x10);
}

static void ioapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(ioapic_base) = reg;
    *(volatile uint32_t*)((uint8_t*)ioapic_base + 0x10) = value;
}

bool ioapic_init(uint32_t base_addr, uint32_t gsi_base) {
    // Map IOAPIC physical address to virtual address
    // Flags: Present | RW (0x3)
    paging_map_page((uint32_t)ioapic_base, base_addr, 0x3);

    ioapic_gsi_base = gsi_base;

    uint32_t ver = ioapic_read(IOAPICVER);
    uint32_t count = ((ver >> 16) & 0xFF) + 1;
    
    serial_printf("[ioapic] init: base=0x%x (phys=0x%x) gsi=%d max_redir=%d\n", 
                  (uint32_t)ioapic_base, base_addr, gsi_base, count);

    return true;
}

void ioapic_enable_irq(uint8_t irq) {
    // Map ISA IRQ to Vector (0x20 + irq)
    // IRQ 0 -> Vector 0x20 (32)
    // IRQ 1 -> Vector 0x21 (33) ...
    uint8_t vector = 0x20 + irq;
    
    // Redirection Table Entry:
    // Vector: 0x20+irq, Delivery: Fixed, Dest: Physical, Polarity: High, Trigger: Edge, Mask: Unmasked
    uint32_t low = vector; 
    uint32_t high = 0; // Dest APIC ID 0 (BSP)

    // Write to IOREDTBL entry (2 registers per entry: low and high)
    ioapic_write(IOREDTBL + 2 * irq, low);
    ioapic_write(IOREDTBL + 2 * irq + 1, high);
    
    serial_printf("[ioapic] IRQ %d -> vector 0x%x\n", irq, vector);
}
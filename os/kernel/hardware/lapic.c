#include "lapic.h"
#include "../paging.h" // Folosim paging.h pentru paging_map_page

// Adresa de bază a LAPIC (mapată virtual)
static volatile uint32_t* lapic_base = (volatile uint32_t*)0xFEE00000;

// Registre LAPIC
#define LAPIC_ID        0x0020
#define LAPIC_VER       0x0030
#define LAPIC_TPR       0x0080
#define LAPIC_EOI 0x00B0
#define LAPIC_SVR       0x00F0
#define LAPIC_ESR       0x0280
#define LAPIC_ICR_LO    0x0300
#define LAPIC_ICR_HI    0x0310
#define LAPIC_TIMER     0x0320
#define LAPIC_PCINT     0x0340
#define LAPIC_LINT0     0x0350
#define LAPIC_LINT1     0x0360
#define LAPIC_ERROR     0x0370
#define LAPIC_TICR      0x0380
#define LAPIC_TCCR      0x0390
#define LAPIC_TDCR      0x03E0

static uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)((uint8_t*)lapic_base + reg);
}

static void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)((uint8_t*)lapic_base + reg) = value;
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

bool lapic_init(uint32_t base_addr) {
    // Mapează adresa fizică a LAPIC la 0xFEE00000
    // Folosim paging_map_page din paging.h (flags: PRESENT | RW | PCD/PWT recomandat, folosim 0x3 basic)
    paging_map_page((uint32_t)lapic_base, base_addr, 0x3);

    // Activează LAPIC: Setare bit 8 în Spurious Interrupt Vector Register (SVR)
    // Setăm vectorul spurious la 0xFF
    lapic_write(LAPIC_SVR, 0x1FF);

    return true;
}

uint32_t lapic_get_id(void) {
    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

void lapic_send_ipi(uint8_t apic_id, uint32_t type, uint8_t vector) {
    // Scrie High DWORD (Destination Field)
    lapic_write(LAPIC_ICR_HI, ((uint32_t)apic_id) << 24);
    
    // Scrie Low DWORD (Type, Vector, Level, etc.)
    lapic_write(LAPIC_ICR_LO, type | vector);
}
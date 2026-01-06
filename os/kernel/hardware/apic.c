#include "apic.h"
#include "lapic.h"
#include "ioapic.h"
#include "acpi.h"
#include "../drivers/serial.h"
#include "../arch/i386/io.h"
#include "../drivers/pic.h" // For PIC disable/enable

static bool apic_active = false;

bool apic_is_active(void) {
    return apic_active;
}

static void pic_disable(void) {
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
    serial_printf("[apic] PIC disabled\n");
}

static void pic_enable(void) {
    // Remap PIC to default (32/40) and unmask
    pic_remap(); 
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
    serial_printf("[apic] PIC re-enabled\n");
}

void apic_init(void) {
    serial_printf("[apic] Initializing APIC subsystem...\n");

    // 1. Detect APIC via MADT. 
    // CRITIC: Verificăm dacă pointerul este valid înainte de orice acces.
    struct MADT* madt = (struct MADT*)acpi_get_madt();
    if (!madt) {
        serial_printf("[apic] MADT not found, staying on PIC\n");
        // STOP: Nu putem continua fără informațiile de topologie din MADT
        return;
    }
    serial_printf("[apic] MADT found\n");
    serial_printf("[apic] LAPIC base = 0x%x\n", madt->local_apic_addr);

    // Find IOAPIC
    struct MADT_IOAPIC* ioapic = 0;
    uint8_t* start = (uint8_t*)madt + sizeof(struct MADT);
    uint8_t* end   = (uint8_t*)madt + madt->h.length;
    
    while (start < end) {
        MADT_Record* rec = (MADT_Record*)start;
        if (rec->type == 1) { // IOAPIC type
            ioapic = (struct MADT_IOAPIC*)rec;
            break;
        }
        start += rec->length;
    }

    if (!ioapic) {
        serial_printf("[apic] IOAPIC not found in MADT, fallback to PIC\n");
        return;
    }
    serial_printf("[apic] IOAPIC id=%d addr=0x%x gsi_base=%d\n", 
                  ioapic->ioapic_id, ioapic->ioapic_addr, ioapic->global_system_interrupt_base);

    // 2. Disable PIC
    pic_disable();

    // 3. Enable Local APIC
    if (!lapic_init(madt->local_apic_addr)) {
        serial_printf("[apic] LAPIC init failed, falling back to PIC\n");
        pic_enable();
        return;
    }
    serial_printf("[apic] LAPIC enabled\n");

    // 4. Initialize IOAPIC
    if (!ioapic_init(ioapic->ioapic_addr, ioapic->global_system_interrupt_base)) {
        serial_printf("[apic] IOAPIC init failed, falling back to PIC\n");
        // Note: LAPIC is already enabled, might need to disable it or just ignore
        pic_enable();
        return;
    }

    // 5. Map standard ISA IRQs (0-15) to vectors 0x20-0x2F
    // This is CRITICAL for keyboard, PIT, and other legacy devices.
    for (int i = 0; i < 16; i++) {
        // Use overrides if present, otherwise default mapping
        // For now, we just map them directly.
        ioapic_enable_irq(i);
    }

    apic_active = true;
    serial_printf("[apic] initialization complete\n");
}
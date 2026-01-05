#include "ioapic.h"
#include "../mm/vmm.h"
#include "../drivers/serial.h"
#include "acpi.h"

#define IOREGSEL 0x00
#define IOWIN    0x10

#define IOAPICID  0x00
#define IOAPICVER 0x01
#define IOAPICARB 0x02
#define IOREDTBL  0x10

static volatile uint32_t* ioapic_base = 0;

static uint32_t ioapic_read(uint32_t reg) {
    *ioapic_base = reg;
    return *(ioapic_base + 4); // IOWIN is at offset 0x10 (4 * 4 bytes)
}

static void ioapic_write(uint32_t reg, uint32_t value) {
    *ioapic_base = reg;
    *(ioapic_base + 4) = value;
}

void ioapic_set_entry(uint8_t index, uint64_t data) {
    ioapic_write(IOREDTBL + index * 2, (uint32_t)data);
    ioapic_write(IOREDTBL + index * 2 + 1, (uint32_t)(data >> 32));
}

bool ioapic_init(uint32_t base_addr, uint32_t gsi_base) {
    ioapic_base = (volatile uint32_t*)base_addr;
    vmm_identity_map(base_addr, 0x1000);
    
    uint32_t ver = ioapic_read(IOAPICVER);
    uint32_t count = ((ver >> 16) & 0xFF) + 1;
    
    serial_printf("[ioapic] version=0x%x max_redir=%d\n", ver & 0xFF, count);

    // Get MADT for ISOs
    struct MADT* madt = (struct MADT*)acpi_get_madt();
    if (!madt) return false;

    // Configure Redirection Table for IRQs 0-15
    for (int i = 0; i < 16; ++i) {
        uint32_t vector = 32 + i; // Map IRQ 0 -> Vector 32
        uint32_t flags = 0; // Fixed delivery, Physical dest, Unmasked
        uint32_t dest = 0; // BSP LAPIC ID (usually 0)
        
        // Check for ISO
        int gsi = i;
        uint16_t iso_flags = 0;
        
        uint8_t* start = (uint8_t*)madt + sizeof(struct MADT);
        uint8_t* end   = (uint8_t*)madt + madt->h.length;
        
        while (start < end) {
            struct MADT_Record* rec = (struct MADT_Record*)start;
            if (rec->type == 2) { // ISO
                struct MADT_IntOverride* iso = (struct MADT_IntOverride*)rec;
                if (iso->bus_source == 0 && iso->irq_source == i) {
                    gsi = iso->global_system_interrupt;
                    iso_flags = iso->flags;
                    serial_printf("[ioapic] ISO found: IRQ %d -> GSI %d\n", i, gsi);
                    break;
                }
            }
            start += rec->length;
        }

        // Apply ISO flags
        // Polarity: 00=Bus def, 01=High, 11=Low
        // Trigger: 00=Bus def, 01=Edge, 11=Level
        // ISA defaults: Edge, High.
        // ACPI spec: Active Low = 1, Active High = 0? No.
        // Bit 13: Polarity (0=High, 1=Low)
        // Bit 15: Trigger (0=Edge, 1=Level)
        
        if ((iso_flags & 0x3) == 0x3) { // Active Low
            flags |= (1 << 13);
        }
        if ((iso_flags & 0xC) == 0xC) { // Level Trigger
            flags |= (1 << 15);
        }

        // Mask all except Keyboard (1) and Timer (0/2) initially for safety?
        // No, we want to replace PIC fully.
        // Note: Timer (IRQ0) is handled by LAPIC Timer now, so we might MASK IRQ0 on IOAPIC
        // to avoid double timer interrupts if PIT is still firing.
        if (i == 0) {
            flags |= (1 << 16); // Mask IRQ0 (PIT) on IOAPIC, use LAPIC Timer
            serial_printf("[ioapic] IRQ 0 (PIT) masked on IOAPIC\n");
        }

        uint64_t entry = vector | flags | ((uint64_t)dest << 56);
        ioapic_set_entry(gsi, entry);
        
        if (i != 0) serial_printf("[ioapic] IRQ %d -> vector 0x%x\n", i, vector);
    }

    return true;
}
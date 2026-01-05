// kernel/hardware/acpi.cpp
#include "acpi.h"
#include "../terminal.h"
#include "../mm/vmm.h"
#include "../drivers/serial.h" // Necesar pentru serial_write_string
#include "../interrupts/irq.h" // Necesar pentru irq_install_handler
#include "../arch/i386/io.h"   // Necesar pentru inb, outb, inw, outw
#include <stddef.h>

/* --- Global ACPI State --- */
struct AcpiState {
    bool enabled;
    bool s5_supported;
    
    // FADT Data
    uint32_t pm1a_cnt;
    uint32_t pm1b_cnt;
    uint16_t slp_typa;
    uint16_t slp_typb;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;

    // MADT Data
    uint8_t cpu_count;
    uint8_t ioapic_count;
    uint32_t lapic_addr;
    
    // Pointers
    FADT* fadt;
    MADT* madt;
};

static AcpiState acpi_state = {0};

/* ============================================================
   Internal Helpers
   ============================================================ */

/* Helper local pentru a scrie hex pe serial (fără printf complex) */
static void serial_print_hex(uint32_t n) {
    const char* digits = "0123456789ABCDEF";
    serial_write_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c[2];
        c[0] = digits[(n >> i) & 0xF];
        c[1] = 0;
        serial_write_string(c);
    }
}

/* Loghează mesajul atât pe ecran cât și pe serial */
static void acpi_log(const char* msg) {
    terminal_writestring("[ACPI] ");
    terminal_writestring(msg);
    terminal_writestring("\n");
    serial_write_string("[ACPI] ");
    serial_write_string(msg);
    serial_write_string("\r\n");
}

/* Helper pentru I/O cu logging (Power Management) */
static void acpi_io_outw(uint16_t port, uint16_t val) {
    serial_write_string("[ACPI] IO WRITE port: ");
    serial_print_hex(port);
    serial_write_string(" val: ");
    serial_print_hex(val);
    serial_write_string("\r\n");
    outw(port, val);
}

static void acpi_io_outb(uint16_t port, uint8_t val) {
    // Logăm doar scrierile critice, nu spam
    outb(port, val);
}

/* 
 * Helper pentru a mapa o regiune fizică în spațiul virtual (identity map).
 * Gestionează alinierea la pagină (4KB) automat.
 */
static void acpi_map_phys(uint32_t phys_addr, uint32_t size)
{
    if (phys_addr == 0 || size == 0) return;

    serial_write_string("[ACPI] Mapping phys: ");
    serial_print_hex(phys_addr);
    serial_write_string(" size: ");
    serial_print_hex(size);
    serial_write_string("\r\n");

    // Rotunjim în jos la începutul paginii (4KB)
    uint32_t base = phys_addr & 0xFFFFF000;
    // Calculăm sfârșitul
    uint32_t end = phys_addr + size;
    
    // Calculăm dimensiunea aliniată (trebuie să acopere tot intervalul)
    // (end + 0xFFF) & 0xFFFFF000 este sfârșitul rotunjit la următoarea pagină
    uint32_t aligned_end = (end + 0xFFF) & 0xFFFFF000;
    uint32_t aligned_size = aligned_end - base;

    vmm_identity_map(base, aligned_size);
    
    serial_write_string("[ACPI] Map call finished.\r\n");
}

/* Verifică suma de control a unui tabel ACPI */
static bool acpi_checksum(void* ptr, uint32_t length)
{
    uint8_t sum = 0;
    uint8_t* p = (uint8_t*)ptr;
    for (uint32_t i = 0; i < length; i++) {
        sum += p[i];
    }
    return (sum == 0);
}

/* ============================================================
   AML / DSDT Minimal Parser (for _S5_)
   ============================================================ */

// Skip PkgLength encoding in AML
static uint8_t* acpi_skip_pkg_length(uint8_t* ptr) {
    uint8_t b = *ptr;
    ptr++;
    if ((b & 0xC0) == 0) {
        return ptr; // 1 byte length (0-63)
    }
    int bytes_to_follow = (b >> 6) & 0x03; // 01=1, 10=2, 11=3 bytes follow
    return ptr + bytes_to_follow;
}

// Parse a generic AML integer (Byte, Word, DWord, Zero, One)
static uint32_t acpi_parse_int(uint8_t** ptr) {
    uint8_t* p = *ptr;
    uint32_t val = 0;

    if (*p == 0x00) { // ZeroOp
        val = 0; p++;
    } else if (*p == 0x01) { // OneOp
        val = 1; p++;
    } else if (*p == 0x0A) { // BytePrefix
        p++; val = *p; p++;
    } else if (*p == 0x0B) { // WordPrefix
        p++; val = *(uint16_t*)p; p += 2;
    } else if (*p == 0x0C) { // DWordPrefix
        p++; val = *(uint32_t*)p; p += 4;
    } else {
        // Unknown or complex type, skip 1 byte and hope
        p++;
    }
    *ptr = p;
    return val;
}

static void acpi_parse_dsdt(uint32_t dsdt_addr) {
    acpi_log("Parsing DSDT for _S5_...");
    
    // Map header first
    acpi_map_phys(dsdt_addr, sizeof(ACPISDTHeader));
    ACPISDTHeader* h = (ACPISDTHeader*)dsdt_addr;
    
    if (h->length < sizeof(ACPISDTHeader) || h->length > 0x100000) {
        acpi_log("Error: DSDT length invalid.");
        return;
    }

    // Map full DSDT
    acpi_map_phys(dsdt_addr, h->length);
    
    uint8_t* start = (uint8_t*)dsdt_addr + sizeof(ACPISDTHeader);
    uint8_t* end = (uint8_t*)dsdt_addr + h->length;

    // Scan for "_S5_" signature: 0x5F 0x53 0x35 0x5F
    // This is a brute-force scan.
    while (start < end - 4) {
        if (start[0] == 0x5F && start[1] == 0x53 && start[2] == 0x35 && start[3] == 0x5F) {
            acpi_log("Found _S5_ object!");
            
            // Check if it's a Package (0x12) or Name(..., Package)
            // Usually: NameOp(0x08) "_S5_" PackageOp(0x12) ...
            // So we look ahead.
            uint8_t* p = start + 4;
            
            // Skip NameOp if we found the string inside a Name definition
            if (*p == 0x12) { // PackageOp
                p++;
                p = acpi_skip_pkg_length(p);
                p++; // NumElements (usually 0x04)
                
                // Parse SLP_TYPa
                acpi_state.slp_typa = (uint16_t)acpi_parse_int(&p);
                // Parse SLP_TYPb
                acpi_state.slp_typb = (uint16_t)acpi_parse_int(&p);
                
                acpi_state.slp_typa <<= 10;
                acpi_state.slp_typb <<= 10;
                acpi_state.s5_supported = true;
                
                serial_write_string("[ACPI] S5 Data - TYPa: ");
                serial_print_hex(acpi_state.slp_typa);
                serial_write_string(" TYPb: ");
                serial_print_hex(acpi_state.slp_typb);
                serial_write_string("\r\n");
                return;
            }
        }
        start++;
    }
    acpi_log("Warning: _S5_ not found in DSDT.");
}

/* ============================================================
   Table Parsers
   ============================================================ */

/* Handler simplu pentru SCI (System Control Interrupt) */
static void acpi_sci_handler(registers_t* r) {
    (void)r;
    // Doar logăm pe serial pentru a nu bloca terminalul
    // serial_write_string("[ACPI] SCI Event detected.\r\n");
    
    // Read status registers to ACK
    if (acpi_state.fadt) {
        uint16_t pm1a_sts = acpi_state.fadt->pm1a_event_block;
        uint16_t pm1b_sts = acpi_state.fadt->pm1b_event_block;
        
        uint16_t val_a = inw(pm1a_sts);
        outw(pm1a_sts, val_a); // Write back to clear
        if (pm1b_sts) {
            uint16_t val_b = inw(pm1b_sts);
            outw(pm1b_sts, val_b);
        }
    }
    // EOI este trimis automat de irq_handler din irq.cpp
}

static void acpi_parse_fadt(ACPISDTHeader* h)
{
    acpi_log("Parsing FADT...");
    
    // Mapăm tot tabelul FADT
    acpi_map_phys((uint32_t)h, h->length);
    FADT* fadt = (FADT*)h;
    acpi_state.fadt = fadt;

    // Store PM Blocks
    acpi_state.pm1a_cnt = fadt->pm1a_control_block;
    acpi_state.pm1b_cnt = fadt->pm1b_control_block;
    acpi_state.smi_cmd  = fadt->smi_command_port;
    acpi_state.acpi_enable = fadt->acpi_enable;
    acpi_state.acpi_disable = fadt->acpi_disable;
    acpi_state.sci_int = fadt->sci_interrupt;

    serial_write_string("[ACPI] SCI Interrupt: ");
    serial_print_hex(fadt->sci_interrupt);
    serial_write_string("\r\n");

    serial_write_string("[ACPI] ACPI Enable: ");
    serial_print_hex(fadt->acpi_enable);
    serial_write_string("\r\n");

    // Instalăm handler-ul SCI dacă este un IRQ valid (ISA IRQ 0-15)
    if (fadt->sci_interrupt > 0 && fadt->sci_interrupt < 16) {
        irq_install_handler(fadt->sci_interrupt, acpi_sci_handler);
        acpi_log("SCI Handler installed.");
    } else {
        acpi_log("SCI Interrupt is not a standard ISA IRQ (GSI > 15), skipping handler.");
    }

    // Enable ACPI if needed
    if (acpi_state.smi_cmd != 0 && acpi_state.acpi_enable != 0) {
        // Check if already enabled (PM1a_CNT_BLK & SCI_EN)
        // Note: SCI_EN is bit 0 of PM1x_CNT usually? No, it's in PM1x_STS or similar.
        // For simplicity, we just write the enable command.
        acpi_log("Enabling ACPI Mode...");
        acpi_io_outb(acpi_state.smi_cmd, acpi_state.acpi_enable);
        
        // Wait a bit (busy loop)
        for(volatile int i=0; i<100000; i++);
        acpi_state.enabled = true;
        acpi_log("ACPI Mode Enabled.");
    }

    // Parse DSDT for S5
    if (fadt->dsdt) {
        acpi_parse_dsdt(fadt->dsdt);
    }
}

static void acpi_parse_madt(ACPISDTHeader* h)
{
    acpi_log("Parsing MADT...");

    // Mapăm tot tabelul MADT
    acpi_map_phys((uint32_t)h, h->length);
    MADT* madt = (MADT*)h;
    acpi_state.madt = madt;
    acpi_state.lapic_addr = madt->local_apic_addr;

    serial_write_string("[ACPI] Local APIC Addr: ");
    serial_print_hex(madt->local_apic_addr);
    serial_write_string("\r\n");

    // Iterăm prin înregistrările MADT (încep după header-ul standard MADT)
    uint8_t* start = (uint8_t*)madt + sizeof(MADT);
    uint8_t* end   = (uint8_t*)madt + madt->h.length;

    while (start < end) {
        MADT_Record* record = (MADT_Record*)start;
        
        // Evităm bucle infinite dacă lungimea e 0 (corupție)
        if (record->length == 0) {
            acpi_log("Error: MADT record length is 0, aborting parse.");
            break;
        }

        switch (record->type) {
            case 0: // Local APIC
            {
                MADT_LocalAPIC* lapic = (MADT_LocalAPIC*)record;
                serial_write_string("  [MADT] Local APIC | ID: ");
                serial_print_hex(lapic->apic_id);
                serial_write_string(lapic->flags & 1 ? " (Enabled)" : " (Disabled)");
                serial_write_string("\r\n");
                if (lapic->flags & 1) acpi_state.cpu_count++;
                break;
            }
            case 1: // I/O APIC
            {
                MADT_IOAPIC* ioapic = (MADT_IOAPIC*)record;
                serial_write_string("  [MADT] I/O APIC   | ID: ");
                serial_print_hex(ioapic->ioapic_id);
                serial_write_string(" Addr: ");
                serial_print_hex(ioapic->ioapic_addr);
                serial_write_string("\r\n");
                acpi_state.ioapic_count++;
                break;
            }
            case 2: // Interrupt Source Override
            {
                MADT_IntOverride* iso = (MADT_IntOverride*)record;
                serial_write_string("  [MADT] ISO        | Bus: ");
                serial_print_hex(iso->bus_source);
                serial_write_string(" IRQ: ");
                serial_print_hex(iso->irq_source);
                serial_write_string(" -> GSI: ");
                serial_print_hex(iso->global_system_interrupt);
                serial_write_string("\r\n");
                break;
            }
            default:
                break;
        }
        start += record->length;
    }
}

/* ============================================================
   RSDP Discovery
   ============================================================ */

static RSDPDescriptor* acpi_scan_memory(uint32_t start, uint32_t length)
{
    serial_write_string("[ACPI] Scanning memory from ");
    serial_print_hex(start);
    serial_write_string(" len ");
    serial_print_hex(length);
    serial_write_string("\r\n");

    // Mapează zona pe care urmează să o scanăm
    acpi_map_phys(start, length);

    // Caută semnătura "RSD PTR " la fiecare 16 bytes
    for (uint32_t addr = start; addr < start + length; addr += 16) {
        RSDPDescriptor* rsdp = (RSDPDescriptor*)addr;
        
        // Verificare rapidă semnătură
        if (rsdp->signature[0] == 'R' && 
            rsdp->signature[1] == 'S' && 
            rsdp->signature[2] == 'D' && 
            rsdp->signature[3] == ' ' &&
            rsdp->signature[4] == 'P' &&
            rsdp->signature[5] == 'T' &&
            rsdp->signature[6] == 'R' &&
            rsdp->signature[7] == ' ') 
        {
            // Verificare checksum
            if (acpi_checksum(rsdp, sizeof(RSDPDescriptor))) {
                return rsdp;
            }
        }
    }
    return nullptr;
}

static RSDPDescriptor* acpi_find_rsdp(void)
{
    // 1. Caută în EBDA (Extended BIOS Data Area)
    acpi_log("Checking EBDA...");
    // Pointerul către EBDA se află la adresa fizică 0x40E
    
    // Mapează pagina 0 pentru a citi BDA
    acpi_map_phys(0, 0x1000);
    
    uint16_t ebda_seg = *(uint16_t*)0x40E;
    uint32_t ebda_phys = (uint32_t)ebda_seg << 4;

    if (ebda_phys != 0 && ebda_phys < 0xA0000) {
        acpi_log("EBDA pointer valid.");
        // EBDA are de obicei 1KB
        RSDPDescriptor* rsdp = acpi_scan_memory(ebda_phys, 1024);
        if (rsdp) return rsdp;
    }

    // 2. Caută în zona principală BIOS (0xE0000 - 0xFFFFF)
    acpi_log("Checking BIOS area (0xE0000)...");
    return acpi_scan_memory(0xE0000, 0x20000);
}

/* ============================================================
   Initialization
   ============================================================ */

void acpi_init(void)
{
    acpi_state.enabled = false;
    acpi_log("Initializing...");

    RSDPDescriptor* rsdp = acpi_find_rsdp();
    if (!rsdp) {
        acpi_log("Error: RSDP not found.");
        return;
    }

    terminal_printf("[acpi] RSDP found at 0x%x, Revision: %d\n", (uint32_t)rsdp, rsdp->revision);
    serial_write_string("[ACPI] RSDP found.\r\n");

    // Verificăm adresa RSDT
    uint32_t rsdt_phys = rsdp->rsdt_address;
    serial_write_string("[ACPI] RSDT Phys Addr: "); serial_print_hex(rsdt_phys); serial_write_string("\r\n");
    if (rsdt_phys == 0) {
        acpi_log("Error: RSDT address is NULL.");
        return;
    }

    // FALLBACK CRITIC: Verificăm dacă RSDT este în zona mapată sigur (120MB)
    // Adresa 0x07800000 = 120 MB. Dacă RSDT e mai sus, VMM-ul actual poate crăpa.
    if (rsdt_phys >= 0x07800000) {
        acpi_log("WARNING: RSDT address > 120MB (outside initial paging).");
        acpi_log("Skipping ACPI init to prevent VMM crash/Kernel Panic.");
        return;
    }

    // 1. Mapăm doar header-ul RSDT pentru a citi lungimea
    acpi_map_phys(rsdt_phys, sizeof(ACPISDTHeader));
    ACPISDTHeader* rsdt = (ACPISDTHeader*)rsdt_phys;

    // Validare semnătură RSDT
    if (rsdt->signature[0] != 'R' || rsdt->signature[1] != 'S' || 
        rsdt->signature[2] != 'D' || rsdt->signature[3] != 'T') {
        acpi_log("Error: Invalid RSDT signature.");
        return;
    }

    // Validare lungime (sanity check: max 4MB)
    if (rsdt->length > 0x400000) {
        terminal_printf("[acpi] Error: RSDT length too big (%d)\n", rsdt->length);
        serial_write_string("[ACPI] Error: RSDT length too big.\r\n");
        return;
    }

    // 2. Mapăm tot tabelul RSDT acum că știm lungimea corectă
    acpi_map_phys(rsdt_phys, rsdt->length);

    if (!acpi_checksum(rsdt, rsdt->length)) {
        acpi_log("Error: RSDT checksum failed.");
        return;
    }

    terminal_printf("[acpi] RSDT at 0x%x mapped, Length: %d\n", rsdt_phys, rsdt->length);
    acpi_log("RSDT mapped and validated.");

    // Iterăm prin intrările RSDT
    // Header-ul are 36 bytes. Intrările sunt pointeri de 32-bit (4 bytes).
    int entries = (rsdt->length - sizeof(ACPISDTHeader)) / 4;
    uint32_t* table_ptrs = (uint32_t*)((uint8_t*)rsdt + sizeof(ACPISDTHeader));

    for (int i = 0; i < entries; i++) {
        uint32_t table_phys = table_ptrs[i];
        if (table_phys == 0) continue;

        // Mapăm header-ul tabelului pentru a-i vedea semnătura
        acpi_map_phys(table_phys, sizeof(ACPISDTHeader));
        ACPISDTHeader* header = (ACPISDTHeader*)table_phys;

        // Afișăm semnătura (ex: FACP, APIC)
        char sig[5];
        sig[0] = header->signature[0];
        sig[1] = header->signature[1];
        sig[2] = header->signature[2];
        sig[3] = header->signature[3];
        sig[4] = '\0';

        terminal_printf("[acpi] Found Table: %s at 0x%x\n", sig, table_phys);
        serial_write_string("[ACPI] Found Table: "); serial_write_string(sig); 
        serial_write_string(" at "); serial_print_hex(table_phys); serial_write_string("\r\n");
        
        // Dispatcher pentru tabele cunoscute
        if (sig[0] == 'A' && sig[1] == 'P' && sig[2] == 'I' && sig[3] == 'C') {
            acpi_parse_madt(header);
        }
        else if (sig[0] == 'F' && sig[1] == 'A' && sig[2] == 'C' && sig[3] == 'P') {
            acpi_parse_fadt(header);
        }
    }

    acpi_log("Initialization complete.");
}

void acpi_shutdown(void) {
    acpi_log("Shutdown requested.");

    if (!acpi_state.s5_supported) {
        acpi_log("Error: S5 not supported or not found in DSDT.");
        return;
    }

    // SLP_EN = 1 << 13 (0x2000)
    uint16_t slp_en = 0x2000;

    acpi_log("Writing to PM1a/b Control Registers...");
    
    // Write PM1a
    if (acpi_state.pm1a_cnt) {
        acpi_io_outw(acpi_state.pm1a_cnt, acpi_state.slp_typa | slp_en);
    }
    
    // Write PM1b
    if (acpi_state.pm1b_cnt) {
        acpi_io_outw(acpi_state.pm1b_cnt, acpi_state.slp_typb | slp_en);
    }

    // Should be off by now
    acpi_log("Shutdown failed (hardware did not react).");
}

uint8_t acpi_get_cpu_count(void) {
    return acpi_state.cpu_count;
}

bool acpi_is_enabled(void) {
    return acpi_state.enabled;
}

void* acpi_get_madt(void) {
    return (void*)acpi_state.madt;
}

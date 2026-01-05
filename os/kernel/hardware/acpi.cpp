// kernel/hardware/acpi.cpp
#include "acpi.h"
#include "../terminal.h"
#include "../paging.h"          // Corect: Folosim direct API-ul de paginare
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
    uint32_t rsdt_phys;
    
    // Pointers
    FADT* fadt;
    MADT* madt;
};

static AcpiState acpi_state = {0};

/* Global APIC Info */
struct ApicInfo apic_info = {0};

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

/* Avem nevoie de page directory pentru unmap. Presupunem simbolul standard. */
extern "C" uint32_t* kernel_page_directory;

// Zonă virtuală rezervată pentru mapări temporare ACPI (trebuie să fie liberă)
// Alegem 0xE0000000 (3.5 GB), departe de kernel (0xC0000000) și heap.
#define ACPI_TEMP_VIRT_BASE 0xE0000000

/* 
 * Helper pentru a mapa TEMPORAR o regiune fizică într-o zonă virtuală fixă.
 * Returnează pointer virtual valid în spațiul kernelului.
 */
static void* acpi_map_temp(uint32_t phys_addr, uint32_t size)
{
    // Permitem phys_addr 0 (necesar pentru BDA), dar nu size 0
    if (size == 0) return nullptr;

    uint32_t phys_page = phys_addr & 0xFFFFF000;
    uint32_t offset    = phys_addr & 0xFFF;
    
    uint32_t end_addr = phys_addr + size;
    uint32_t end_page = (end_addr + 0xFFF) & 0xFFFFF000;
    uint32_t map_size = end_page - phys_page;
    uint32_t num_pages = map_size / 0x1000;

    // Folosim zona fixă ACPI_TEMP_VIRT_BASE
    // ATENȚIE: Această implementare simplă nu suportă re-entranță sau mapări multiple simultane
    // care depășesc zona alocată, dar pentru ACPI init secvențial e ok.
    // Totuși, pentru a permite mapări imbricate (ex: FADT mapat în timp ce RSDT e mapat),
    // ar trebui să incrementăm baza. Aici simplificăm și presupunem că unmap se face corect.
    // Pentru siguranță, vom folosi o variabilă statică pentru a avansa baza dacă e nevoie,
    // sau pur și simplu mapăm la ACPI_TEMP_VIRT_BASE și sperăm că nu se suprapun.
    // Dat fiind că acpi_init face unmap imediat, riscul e mic, dar RSDT rămâne mapat
    // în timp ce iterăm. Deci avem nevoie de o bază dinamică simplă.
    
    static uint32_t current_virt_offset = 0;
    uint32_t virt_addr = ACPI_TEMP_VIRT_BASE + current_virt_offset;

    // Avansăm offset-ul pentru următoarea mapare (simplu bump allocator)
    // Resetăm offset-ul dacă e prea mare (hacky, dar previne overflow la boot)
    if (current_virt_offset > 0x100000) current_virt_offset = 0; 
    current_virt_offset += map_size;

    serial_write_string("[ACPI] map phys page: ");
    serial_print_hex(phys_page);
    serial_write_string(" -> virt page: ");
    serial_print_hex(virt_addr);
    serial_write_string("\r\n");

    if (!kernel_page_directory) {
        serial_write_string("[ACPI] CRITICAL: kernel_page_directory is NULL!\r\n");
        // If paging isn't ready, we can't map. Return NULL to trigger error handling upstream.
        return nullptr; 
    }

    // Mapăm paginile fizice la adresa virtuală aleasă
    for (uint32_t i = 0; i < num_pages; i++) {
        paging_map_page(virt_addr + i * 0x1000, phys_page + i * 0x1000, PAGE_PRESENT | PAGE_RW);
    }
    
    return (void*)(virt_addr + offset);
}

/* Demontează maparea temporară pentru a nu polua spațiul virtual */
static void acpi_unmap_temp(void* ptr, uint32_t size) {
    uint32_t virt = (uint32_t)ptr;
    
    // Protecție: Unmap doar dacă e în zona noastră temporară
    if (virt < ACPI_TEMP_VIRT_BASE) return;

    uint32_t base = virt & 0xFFFFF000;
    uint32_t end = virt + size;
    uint32_t aligned_end = (end + 0xFFF) & 0xFFFFF000;

    for (uint32_t addr = base; addr < aligned_end; addr += 0x1000) {
        paging_unmap_page(addr);
    }
    serial_write_string("[ACPI] Unmapped temp region.\r\n");
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
    ACPISDTHeader* h = (ACPISDTHeader*)acpi_map_temp(dsdt_addr, sizeof(ACPISDTHeader));
    
    if (h->length < sizeof(ACPISDTHeader) || h->length > 0x100000) {
        acpi_log("Error: DSDT length invalid.");
        acpi_unmap_temp(h, sizeof(ACPISDTHeader));
        return;
    }

    // Map full DSDT
    uint8_t* dsdt_virt = (uint8_t*)acpi_map_temp(dsdt_addr, h->length);
    
    uint8_t* start = dsdt_virt + sizeof(ACPISDTHeader);
    uint8_t* end = dsdt_virt + h->length;

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
                acpi_unmap_temp(dsdt_virt, h->length);
                return;
            }
        }
        start++;
    }
    acpi_log("Warning: _S5_ not found in DSDT.");
    acpi_unmap_temp(dsdt_virt, h->length);
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

static void acpi_parse_fadt(uint32_t phys_addr, uint32_t length)
{
    acpi_log("Parsing FADT...");
    
    // Mapăm tot tabelul FADT
    FADT* fadt = (FADT*)acpi_map_temp(phys_addr, length);
    acpi_state.fadt = fadt;

    uint32_t pages = (length + (phys_addr & 0xFFF) + 0xFFF) / 0x1000;
    serial_write_string("[ACPI] FADT length: "); serial_print_hex(length); serial_write_string("\r\n");
    serial_write_string("[ACPI] FADT pages mapped: "); serial_print_hex(pages); serial_write_string("\r\n");

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
    
    // Nu facem unmap la FADT imediat deoarece avem nevoie de pointerii din el (pm1a etc)
    // TODO: Copiază datele și fă unmap. Momentan îl lăsăm mapat (leak mic).
}

static void acpi_parse_madt(uint32_t phys_addr, uint32_t length)
{
    acpi_log("Parsing MADT...");

    // Mapăm tot tabelul MADT (header-ul e deja mapat de caller, dar extindem maparea)
    MADT* madt = (MADT*)acpi_map_temp(phys_addr, length);
    acpi_state.madt = madt;
    acpi_state.lapic_addr = madt->local_apic_addr;
    apic_info.lapic_addr = madt->local_apic_addr;

    uint32_t pages = (length + (phys_addr & 0xFFF) + 0xFFF) / 0x1000;
    serial_write_string("[ACPI] MADT length: "); serial_print_hex(length); serial_write_string("\r\n");
    serial_write_string("[ACPI] MADT pages mapped: "); serial_print_hex(pages); serial_write_string("\r\n");

    serial_write_string("[APIC] LAPIC @ ");
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
                serial_write_string("[APIC] IOAPIC @ ");
                serial_print_hex(ioapic->ioapic_addr);
                serial_write_string(" ID: ");
                serial_print_hex(ioapic->ioapic_id);
                serial_write_string("\r\n");
                acpi_state.ioapic_count++;
                
                if (apic_info.ioapic_count < MAX_IOAPICS) {
                    int idx = apic_info.ioapic_count++;
                    apic_info.ioapics[idx].id = ioapic->ioapic_id;
                    apic_info.ioapics[idx].addr = ioapic->ioapic_addr;
                    apic_info.ioapics[idx].gsi_base = ioapic->global_system_interrupt_base;
                }
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
                
                if (apic_info.override_count < MAX_OVERRIDES) {
                    int idx = apic_info.override_count++;
                    apic_info.overrides[idx].bus = iso->bus_source;
                    apic_info.overrides[idx].irq = iso->irq_source;
                    apic_info.overrides[idx].gsi = iso->global_system_interrupt;
                    apic_info.overrides[idx].flags = iso->flags;
                }
                break;
            }
            default:
                break;
        }
        start += record->length;
    }
    
    // NU demontăm maparea MADT!
    // Pointerul 'madt' este salvat în acpi_state.madt și folosit ulterior de apic_init.
    // Deoarece folosim un allocator bump în zona temporară, pointerul rămâne valid.
    // acpi_unmap_temp(madt, length);
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

    // Mapează zona pe care urmează să o scanăm și obține pointer virtual valid
    uint8_t* virt_start = (uint8_t*)acpi_map_temp(start, length);
    if (!virt_start) {
        serial_write_string("[ACPI] Scan failed: map returned NULL\r\n");
        return nullptr;
    }

    // Caută semnătura "RSD PTR " la fiecare 16 bytes
    for (uint32_t offset = 0; offset < length; offset += 16) {
        RSDPDescriptor* rsdp = (RSDPDescriptor*)(virt_start + offset);
        
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
                // SAFE COPY: Copiem structura într-un buffer static pentru a putea face unmap
                static RSDPDescriptor rsdp_cache;
                __builtin_memcpy(&rsdp_cache, rsdp, sizeof(RSDPDescriptor));
                
                acpi_unmap_temp(virt_start, length);
                return &rsdp_cache;
            }
        }
    }
    
    acpi_unmap_temp(virt_start, length);
    return nullptr;
}

static RSDPDescriptor* acpi_find_rsdp(void)
{
    // 1. Caută în EBDA (Extended BIOS Data Area)
    acpi_log("Checking EBDA...");
    // Pointerul către EBDA se află la adresa fizică 0x40E
    
    // Mapează pagina 0 pentru a citi BDA
    uint16_t* bda_virt = (uint16_t*)acpi_map_temp(0, 0x1000);
    if (!bda_virt) {
        acpi_log("Failed to map BDA");
        return nullptr;
    }
    
    // 0x40E este offset în pagina 0
    uint16_t ebda_seg = *(uint16_t*)((uint8_t*)bda_virt + 0x40E);
    acpi_unmap_temp(bda_virt, 0x1000);

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
    // DISABLE INTERRUPTS
    asm volatile("cli");
    // serial_write_string("[ACPI] interrupts disabled\r\n");

    acpi_state.enabled = false;
    acpi_log("Initializing...");

    // Resetăm pointerii globali pentru a evita folosirea datelor vechi/invalide
    acpi_state.madt = nullptr;
    acpi_state.fadt = nullptr;

    if (!kernel_page_directory) {
        serial_write_string("[ACPI] FATAL: paging not initialized (kernel_page_directory is NULL)\r\n");
        asm volatile("sti");
        return;
    }

    RSDPDescriptor* rsdp = acpi_find_rsdp();
    if (!rsdp) {
        acpi_log("Error: RSDP not found.");
        // STOP TOTAL: Nu continuăm dacă nu avem punctul de intrare ACPI
        asm volatile("sti");
        return;
    }

    terminal_printf("[acpi] RSDP found at 0x%x, Revision: %d\n", (uint32_t)rsdp, rsdp->revision);
    serial_write_string("[ACPI] RSDP found.\r\n");

    // Verificăm adresa RSDT
    uint32_t rsdt_phys = rsdp->rsdt_address;
    serial_write_string("[ACPI] RSDT Phys Addr: "); serial_print_hex(rsdt_phys); serial_write_string("\r\n");
    acpi_state.rsdt_phys = rsdt_phys;
    if (rsdt_phys == 0) {
        acpi_log("Error: RSDT address is NULL.");
        // serial_write_string("[ACPI] interrupts restored\r\n");
        asm volatile("sti");
        return;
    }

    // 1. Mapăm doar header-ul RSDT pentru a citi lungimea
    ACPISDTHeader* rsdt_header = (ACPISDTHeader*)acpi_map_temp(rsdt_phys, sizeof(ACPISDTHeader));

    // DEBUG: Afișăm ce citim de la adresa mapată pentru a confirma validitatea
    char sig_debug[5];
    sig_debug[0] = rsdt_header->signature[0];
    sig_debug[1] = rsdt_header->signature[1];
    sig_debug[2] = rsdt_header->signature[2];
    sig_debug[3] = rsdt_header->signature[3];
    sig_debug[4] = 0;
    serial_write_string("[ACPI] RSDT virt: "); serial_print_hex((uint32_t)rsdt_header); 
    serial_write_string("\r\n[ACPI] RSDT sig read: "); serial_write_string(sig_debug); serial_write_string("\r\n");

    // Validare semnătură RSDT
    if (rsdt_header->signature[0] != 'R' || rsdt_header->signature[1] != 'S' || 
        rsdt_header->signature[2] != 'D' || rsdt_header->signature[3] != 'T') {
        acpi_log("Error: Invalid RSDT signature.");
        acpi_unmap_temp(rsdt_header, sizeof(ACPISDTHeader));
        // serial_write_string("[ACPI] interrupts restored\r\n");
        asm volatile("sti");
        return;
    }

    uint32_t rsdt_len = rsdt_header->length;
    acpi_unmap_temp(rsdt_header, sizeof(ACPISDTHeader)); // Unmap initial

    // Validare lungime (sanity check: max 4MB)
    if (rsdt_len > 0x400000) {
        terminal_printf("[acpi] Error: RSDT length too big (%d)\n", rsdt_len);
        serial_write_string("[ACPI] Error: RSDT length too big.\r\n");
        // serial_write_string("[ACPI] interrupts restored\r\n");
        asm volatile("sti");
        return;
    }

    // 2. Mapăm tot tabelul RSDT acum că știm lungimea corectă
    ACPISDTHeader* rsdt = (ACPISDTHeader*)acpi_map_temp(rsdt_phys, rsdt_len);
    
    uint32_t pages = (rsdt_len + (rsdt_phys & 0xFFF) + 0xFFF) / 0x1000;
    serial_write_string("[ACPI] RSDT length: "); serial_print_hex(rsdt_len); serial_write_string("\r\n");
    serial_write_string("[ACPI] RSDT pages mapped: "); serial_print_hex(pages); serial_write_string("\r\n");

    if (!acpi_checksum(rsdt, rsdt_len)) {
        acpi_log("Error: RSDT checksum failed.");
        acpi_unmap_temp(rsdt, rsdt_len);
        // serial_write_string("[ACPI] interrupts restored\r\n");
        asm volatile("sti");
        return;
    }

    terminal_printf("[acpi] RSDT at 0x%x mapped, Length: %d\n", rsdt_phys, rsdt_len);
    acpi_log("RSDT mapped temporarily.");

    // Iterăm prin intrările RSDT
    int entries = (rsdt_len - sizeof(ACPISDTHeader)) / 4;
    uint32_t* table_ptrs = (uint32_t*)((uint8_t*)rsdt + sizeof(ACPISDTHeader));

    for (int i = 0; i < entries; i++) {
        uint32_t table_phys = table_ptrs[i];
        if (table_phys == 0) continue;

        // Mapăm header-ul tabelului pentru a-i vedea semnătura
        ACPISDTHeader* header = (ACPISDTHeader*)acpi_map_temp(table_phys, sizeof(ACPISDTHeader));

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
            acpi_parse_madt(table_phys, header->length);
        }
        else if (sig[0] == 'F' && sig[1] == 'A' && sig[2] == 'C' && sig[3] == 'P') {
            acpi_parse_fadt(table_phys, header->length);
        }
        
        // Unmap header (dacă parserul a mapat tot tabelul, el a făcut unmap la tot, deci e ok)
        acpi_unmap_temp(header, sizeof(ACPISDTHeader));
    }

    acpi_unmap_temp(rsdt, rsdt_len);
    acpi_log("Initialization complete.");

    // serial_write_string("[ACPI] interrupts restored\r\n");
    asm volatile("sti");
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
    // Returnează NULL dacă MADT nu a fost parsat cu succes
    return (void*)acpi_state.madt;
}

extern "C" void* acpi_find_table(const char* signature) {
    if (!acpi_state.rsdt_phys) return nullptr;

    // 1. Mapăm header-ul RSDT pentru a afla lungimea
    ACPISDTHeader* rsdt_header = (ACPISDTHeader*)acpi_map_temp(acpi_state.rsdt_phys, sizeof(ACPISDTHeader));
    if (!rsdt_header) return nullptr;
    
    uint32_t rsdt_len = rsdt_header->length;
    acpi_unmap_temp(rsdt_header, sizeof(ACPISDTHeader));

    // 2. Mapăm tot RSDT-ul
    ACPISDTHeader* rsdt = (ACPISDTHeader*)acpi_map_temp(acpi_state.rsdt_phys, rsdt_len);
    if (!rsdt) return nullptr;

    int entries = (rsdt_len - sizeof(ACPISDTHeader)) / 4;
    uint32_t* table_ptrs = (uint32_t*)((uint8_t*)rsdt + sizeof(ACPISDTHeader));
    
    void* found_table = nullptr;

    for (int i = 0; i < entries; i++) {
        uint32_t table_phys = table_ptrs[i];
        if (table_phys == 0) continue;

        ACPISDTHeader* header = (ACPISDTHeader*)acpi_map_temp(table_phys, sizeof(ACPISDTHeader));
        if (!header) continue;

        if (header->signature[0] == signature[0] &&
            header->signature[1] == signature[1] &&
            header->signature[2] == signature[2] &&
            header->signature[3] == signature[3]) 
        {
            uint32_t len = header->length;
            acpi_unmap_temp(header, sizeof(ACPISDTHeader));
            found_table = acpi_map_temp(table_phys, len);
            break;
        }
        acpi_unmap_temp(header, sizeof(ACPISDTHeader));
    }

    acpi_unmap_temp(rsdt, rsdt_len);
    return found_table;
}

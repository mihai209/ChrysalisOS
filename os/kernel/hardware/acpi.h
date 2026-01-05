#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
   ACPI Data Structures (Packed)
   ============================================================ */

struct RSDPDescriptor {
    char     signature[8];   // "RSD PTR "
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;       // 0 = ACPI 1.0, 2 = ACPI 2.0+
    uint32_t rsdt_address;   // Physical address of RSDT
} __attribute__((packed));

struct ACPISDTHeader {
    char     signature[4];   // e.g. "APIC", "FACP"
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

/* --- MADT (Multiple APIC Description Table) --- */
struct MADT {
    ACPISDTHeader h;
    uint32_t local_apic_addr;
    uint32_t flags;
} __attribute__((packed));

struct MADT_Record {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct MADT_LocalAPIC {
    MADT_Record header;
    uint8_t  processor_id;
    uint8_t  apic_id;
    uint32_t flags;
} __attribute__((packed));

struct MADT_IOAPIC {
    MADT_Record header;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_addr;
    uint32_t global_system_interrupt_base;
} __attribute__((packed));

struct MADT_IntOverride {
    MADT_Record header;
    uint8_t  bus_source;
    uint8_t  irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed));

/* --- FADT (Fixed ACPI Description Table) --- */
struct FADT {
    ACPISDTHeader h;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_length;
    uint8_t  gpe1_length;
    uint8_t  gpe1_base;
    uint8_t  cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;
} __attribute__((packed));

/* Inițializează subsistemul ACPI:
   - Caută RSDP
   - Parsează RSDT
   - Mapează tabelele în memorie
   - Parsează MADT (APIC) și FADT
   - Instalează handler pentru SCI (System Control Interrupt)
   - Loghează erorile pe portul serial pentru debug
*/
void acpi_init(void);

/* Încearcă oprirea sistemului folosind ACPI (S5 Soft Off) */
void acpi_shutdown(void);

/* Returnează numărul de CPU-uri detectate în MADT */
uint8_t acpi_get_cpu_count(void);

/* Returnează true dacă ACPI este activ și funcțional */
bool acpi_is_enabled(void);

/* Returnează pointer la tabela MADT parsat (sau NULL) */
void* acpi_get_madt(void);

#ifdef __cplusplus
}
#endif

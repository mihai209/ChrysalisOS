#include "smp.h"
#include "../hardware/acpi.h"
#include "../hardware/lapic.h"
#include "../hardware/apic.h"
#include "../drivers/serial.h"
#include "../drivers/pit.h"
#include "../arch/i386/gdt.h"
#include "../arch/i386/idt.h"
#include "../paging.h" // For paging_map_page

cpu_info_t cpus[MAX_CPUS];
volatile int cpu_count = 0;

/*
 * Main C entry point for Application Processors (APs).
 * This function is called by the trampoline code.
 */

/* Stacks for the APs. 4KB per stack. */
__attribute__((aligned(16)))
static uint8_t ap_stacks[MAX_CPUS - 1][4096];

/*
 * This is the C function that Application Processors jump to after
 * the trampoline code has finished.
 */
void ap_main(void) {
    serial_write_string("[AP] ap_main entered\r\n");

    uint32_t id = lapic_get_id();
    serial_printf("[SMP] AP started! APIC ID=%d\n", id);

    // Each AP needs its own GDT and IDT
    // NOTE: These functions must be AP-safe.
    // If they modify global state without locks, you'll have races.
    // APs should just LOAD the existing GDT/IDT, not re-init them.
    // Assuming gdt_flush/idt_flush or similar are available, or just rely on trampoline GDT for now.
    // For now, we assume trampoline set up a basic GDT.
    // Ideally: load_gdt(gdt_ptr); load_idt(idt_ptr);

    // Enable APIC on this core
    // The base address is the same for all cores.
    struct MADT* madt = (struct MADT*)acpi_get_madt();
    if (madt) {
        lapic_init(madt->local_apic_addr);
    }

    // Mark this CPU as online
    for (int i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == id) {
            cpus[i].online = 1;
            break;
        }
    }

    serial_printf("[SMP] AP %d online\n", id);

    // Enable interrupts on this core
    asm volatile("sti");

    // Halt until an interrupt arrives
    while (1) {
        asm volatile("hlt");
    }
}

void smp_detect_cpus(void) {
    serial_printf("[SMP] Detecting CPUs via MADT...\n");

    struct MADT* madt = (struct MADT*)acpi_get_madt();
    if (!madt) {
        serial_printf("[SMP] MADT not found. Cannot detect CPUs.\n");
        cpu_count = 1; // Only BSP
        cpus[0].apic_id = lapic_get_id();
        cpus[0].online = 1;
        return;
    }

    uint8_t* start = (uint8_t*)madt + sizeof(struct MADT);
    uint8_t* end   = (uint8_t*)madt + ((struct MADT*)madt)->h.length;

    while (start < end) {
        MADT_Record* rec = (MADT_Record*)start;
        if (rec->type == 0 && cpu_count < MAX_CPUS) { // Local APIC
            struct MADT_LocalAPIC* lapic = (struct MADT_LocalAPIC*)rec;
            if (lapic->flags & 1) { // Enabled
                cpus[cpu_count].apic_id = lapic->apic_id;
                cpus[cpu_count].online = (cpu_count == 0); // BSP is initially online
                serial_printf("[SMP] Found CPU %d (APIC ID=%d)\n", cpu_count, lapic->apic_id);
                cpu_count++;
            }
        }
        start += rec->length;
    }

    if (cpu_count == 0) {
        serial_printf("[SMP] Warning: No CPUs found in MADT, assuming 1 (BSP).\n");
        cpu_count = 1;
        cpus[0].apic_id = lapic_get_id();
        cpus[0].online = 1;
    }

    serial_printf("[SMP] Total CPUs detected: %d\n", cpu_count);
}

/* Symbols from the linked trampoline binary */
extern uint8_t _binary_build_smp_trampoline_bin_start[];
extern uint8_t _binary_build_smp_trampoline_bin_end[];
extern uint8_t _binary_build_smp_trampoline_bin_size[];

/*
 * Prepares the trampoline and data structures needed for AP startup.
 * 1. Copies the linked trampoline binary to 0x7000.
 * 2. Copies it to the low physical address 0x7000.
 * 3. Patches the trampoline with the GDT pointer.
 */
bool smp_prepare_aps(void) {
    serial_printf("[SMP] Preparing for AP startup...\n");

    uint32_t trampoline_size = (uint32_t)_binary_build_smp_trampoline_bin_size;
    uint8_t* trampoline_src = _binary_build_smp_trampoline_bin_start;

    // Step 2: Copy trampoline to 0x7000
    // We need to ensure 0x7000 is mapped. Since we are in kernel, we can use identity map if available
    // or map it temporarily. Assuming 0x7000 is identity mapped in lower memory.
    // If not, we must map it.
    // paging_map_page(0x7000, 0x7000, PAGE_PRESENT | PAGE_RW); // Ensure it's mapped
    
    uint8_t* dest = (uint8_t*)0x7000;
    uint8_t* src = trampoline_src;
    for (uint32_t i = 0; i < trampoline_size; i++) {
        dest[i] = src[i];
    }
    serial_printf("[SMP] Trampoline code copied to 0x%x\n", (uint32_t)dest);

    // Step 3: Prepare and patch GDT descriptor for the trampoline
    // The trampoline needs a GDT to switch to protected mode. We give it the BSP's GDT.
    gdt_ptr_t gdt_ptr;
    gdt_get_ptr(&gdt_ptr);

    // The trampoline expects a 6-byte GDTR structure at the 'gdt_desc' label.
    // We need to find that label's address inside the 0x7000 page.
    // This is tricky without symbols. A common way is to put it at a known offset.
    // Based on trampoline.S structure (data at the end):
    // ap_entry_ptr (4 bytes) is last.
    // ap_cr3_ptr (4 bytes) is before that.
    // ap_stack_ptr (4 bytes) is before that.
    // gdt_desc (6 bytes) is before that.
    // Total data size = 18 bytes.
    
    uint8_t* data_end = dest + trampoline_size;
    
    uint16_t* gdt_desc_limit = (uint16_t*)(data_end - 18);
    uint32_t* gdt_desc_base  = (uint32_t*)(data_end - 16);
    uint32_t* ap_stack_ptr   = (uint32_t*)(data_end - 12); // Placeholder, patched per-CPU
    uint32_t* ap_cr3_ptr     = (uint32_t*)(data_end - 8);
    uint32_t* ap_entry_ptr   = (uint32_t*)(data_end - 4);

    *gdt_desc_limit = gdt_ptr.limit;
    *gdt_desc_base = gdt_ptr.base;
    *ap_cr3_ptr = paging_get_page_directory_phys(); // Use physical address of PD
    *ap_entry_ptr = (uint32_t)ap_main;

    serial_printf("[SMP] Patched trampoline: GDT base=0x%x, Entry=0x%x\n", gdt_ptr.base, *ap_entry_ptr);

    return true;
}

/* Simple busy wait to avoid PIT interrupt dependency during SMP bringup */
static void smp_busy_wait(int count) {
    for (volatile int i = 0; i < count * 10000; i++) {
        asm volatile("nop");
    }
}

/*
 * Starts all Application Processors (APs) using the INIT-SIPI-SIPI sequence.
 * The trampoline code must be loaded at 0x7000 before calling this.
 */
void smp_start_aps(void) {
    uint32_t trampoline_phys = 0x7000;
    uint8_t vector = (trampoline_phys >> 12); // Vector 0x07 for 0x7000

    serial_write_string("[SMP] Starting APs...\n");

    // Iterate through detected CPUs
    for (int i = 0; i < cpu_count; i++) {
        uint8_t apic_id = cpus[i].apic_id;

        // Skip BSP (Bootstrap Processor) - usually APIC ID 0, but check logic if needed
        // We assume the CPU running this code is the BSP.
        if (apic_id == lapic_get_id()) continue;

        // Assign a stack for this AP and patch the trampoline
        uint32_t stack_top = (uint32_t)ap_stacks[i] + sizeof(ap_stacks[i]);
        
        // Patch the stack pointer in the trampoline
        // ap_stack_ptr is at offset -12 from the end (see above calculation)
        uint32_t trampoline_size = (uint32_t)_binary_build_smp_trampoline_bin_size;
        uint32_t* ap_stack_ptr = (uint32_t*)(0x7000 + trampoline_size - 12);
        *ap_stack_ptr = stack_top;
        
        serial_printf("[SMP] Assigning stack 0x%x for APIC ID %d\n", stack_top, apic_id);

        serial_printf("[SMP] Sending INIT IPI to CPU %d\n", apic_id);
        lapic_send_ipi(apic_id, 5, 0); // INIT (Type 5), Vector 0
        
        serial_write_string("[SMP] After INIT, waiting...\r\n");
        // Wait 10ms
        smp_busy_wait(100); // Approx 10ms

        serial_printf("[SMP] Sending SIPI #1 to CPU %d (Vector 0x%x)\n", apic_id, vector);
        lapic_send_ipi(apic_id, 6, vector); // SIPI (Type 6)
        
        smp_busy_wait(2); // Wait > 200 microseconds

        serial_printf("[SMP] Sending SIPI #2 to CPU %d (Vector 0x%x)\n", apic_id, vector);
        lapic_send_ipi(apic_id, 6, vector); // SIPI (Type 6)
        smp_busy_wait(2);
    }
}
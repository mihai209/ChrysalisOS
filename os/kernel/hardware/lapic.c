#include "lapic.h"
#include "msr.h"
#include "../mm/vmm.h"
#include "../drivers/serial.h"
#include "../arch/i386/io.h"
#include "../time/timer.h" // For PIT fallback/calibration

#define LAPIC_ID        0x0020
#define LAPIC_VER       0x0030
#define LAPIC_TPR       0x0080
#define LAPIC_EOI       0x00B0
#define LAPIC_SVR       0x00F0
#define LAPIC_ESR       0x0280
#define LAPIC_LVT_CMCI  0x02F0
#define LAPIC_ICR_LO    0x0300
#define LAPIC_ICR_HI    0x0310
#define LAPIC_LVT_TIMER 0x0320
#define LAPIC_LVT_LINT0 0x0350
#define LAPIC_LVT_LINT1 0x0360
#define LAPIC_LVT_ERR   0x0370
#define LAPIC_TICR      0x0380  // Initial Count
#define LAPIC_TCCR      0x0390  // Current Count
#define LAPIC_TDCR      0x03E0  // Divide Config

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

static volatile uint32_t* lapic_base = 0;

static uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t*)((uintptr_t)lapic_base + reg));
}

static void lapic_write(uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)((uintptr_t)lapic_base + reg)) = value;
}

void lapic_eoi(void) {
    if (lapic_base) {
        lapic_write(LAPIC_EOI, 0);
    }
}

uint32_t lapic_get_id(void) {
    if (!lapic_base) return 0;
    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

static void lapic_timer_calibrate(void) {
    serial_printf("[apic] Calibrating APIC timer...\n");

    // 1. Configure APIC Timer: Divide by 16, One-Shot
    lapic_write(LAPIC_TDCR, 0x3); // Divide by 16
    lapic_write(LAPIC_LVT_TIMER, 0x10000); // Masked for now
    
    // 2. Prepare PIT for 10ms wait (100Hz)
    // PIT Frequency = 1193182 Hz. 10ms = 11931 ticks.
    uint32_t pit_ticks = 11931;
    
    outb(0x43, 0x30); // Channel 0, Mode 0, Lo/Hi
    outb(0x40, pit_ticks & 0xFF);
    outb(0x40, (pit_ticks >> 8) & 0xFF);

    // 3. Start APIC Timer with max count
    lapic_write(LAPIC_TICR, 0xFFFFFFFF);

    // 4. Wait for PIT to reach 0
    // Note: This assumes PIC is disabled/masked so no IRQ0 fires.
    // We poll the PIT status or count.
    // Mode 0: Output goes high when count reaches 0.
    // We can check port 0x61 bit 5 (OUT 2) if using Ch2, but we used Ch0.
    // Simpler: Read back count until it wraps or is large (Mode 0 counts down and stops).
    // Actually, Mode 0 counts down to 0.
    
    while (1) {
        outb(0x43, 0x00); // Latch command
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        uint16_t count = lo | (hi << 8);
        if (count > pit_ticks) break; // Wrapped/Stopped (undefined behavior in mode 0 after 0?)
        // Actually, let's just wait a safe amount of loops if reading PIT is flaky
        // Better: Use the fact that we are in kernel init and can spin.
        if (count == 0) break; // Reached 0
    }

    // 5. Stop APIC Timer
    lapic_write(LAPIC_LVT_TIMER, 0x10000); // Mask
    
    uint32_t curr_count = lapic_read(LAPIC_TCCR);
    uint32_t ticks_in_10ms = 0xFFFFFFFF - curr_count;
    
    // 6. Calculate ticks for 1ms (scheduler tick)
    // We measured 10ms.
    uint32_t ticks_per_ms = ticks_in_10ms / 10;
    
    serial_printf("[apic] APIC timer calibrated: %d ticks/ms\n", ticks_per_ms);

    // 7. Start APIC Timer: Periodic, Vector 32 (IRQ0 equivalent)
    lapic_write(LAPIC_TDCR, 0x3); // Divide by 16
    lapic_write(LAPIC_LVT_TIMER, 32 | 0x20000); // Vector 32, Periodic mode
    lapic_write(LAPIC_TICR, ticks_per_ms); // 1ms interval
    
    serial_printf("[apic] APIC timer started (periodic)\n");
}

bool lapic_init(uint32_t base_addr) {
    if (!cpu_has_msr()) {
        serial_printf("[apic] Error: CPU does not support MSR\n");
        return false;
    }

    // 1. Enable APIC via MSR
    uint32_t lo, hi;
    rdmsr(IA32_APIC_BASE_MSR, &lo, &hi);
    if (!(lo & IA32_APIC_BASE_MSR_ENABLE)) {
        serial_printf("[apic] Enabling APIC via MSR...\n");
        lo |= IA32_APIC_BASE_MSR_ENABLE;
        wrmsr(IA32_APIC_BASE_MSR, lo, hi);
    }

    // 2. Map LAPIC
    lapic_base = (volatile uint32_t*)base_addr;
    vmm_identity_map(base_addr, 0x1000); // Map 4KB
    serial_printf("[apic] LAPIC mapped at 0x%x\n", base_addr);

    // 3. Init Spurious Interrupt Vector (Enable APIC software)
    // Vector 0xFF, Bit 8 = Enable
    lapic_write(LAPIC_SVR, 0x1FF);
    serial_printf("[apic] spurious vector = 0xFF\n");

    // 4. Configure LVT
    lapic_write(LAPIC_TPR, 0); // Accept all interrupts
    lapic_write(LAPIC_LVT_LINT0, 0x10000); // Mask LINT0
    lapic_write(LAPIC_LVT_LINT1, 0x10000); // Mask LINT1
    lapic_write(LAPIC_LVT_ERR, 0x10000);   // Mask Error
    
    // Ack any pending interrupts
    lapic_write(LAPIC_EOI, 0);
    
    serial_printf("[apic] LAPIC registers configured\n");

    // 5. Calibrate and start Timer
    lapic_timer_calibrate();

    return true;
}
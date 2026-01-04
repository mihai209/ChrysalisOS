// kernel/kernel.cpp
// Updated: defensive task integration + panic fallback (tasks disabled by default)
//
// Notes:
//  - TASKS_ENABLED = 0 by default to avoid bootloops caused by incomplete context-switch.
//  - To re-enable: set TASKS_ENABLED to 1, ensure you have a correct arch/i386/switch.S
//    that saves/restores full CPU context (pushad/popad) and returns to a valid stack.
//  - Triple faults cannot be caught — avoid them by not switching into invalid stacks.
//  - For harder safety, implement a double-fault handler in your IDT that calls panic()
//    so many kernel faults will show a panic screen instead of resulting in a triple fault.

#include "types.h"

#include "terminal.h"
#include "drivers/pic.h"
#include "arch/i386/gdt.h"
#include "arch/i386/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "shell/shell.h"
#include "fs/fs.h"
#include "bootlogo/bootlogo.h"
#include "drivers/serial.h"
#include "drivers/rtc.h"
#include "time/clock.h"
#include "drivers/audio/audio.h"
#include "drivers/mouse.h"
#include "input/keyboard_buffer.h"
#include "video/vga.h"
#include "time/timer.h"
#include "events/event_queue.h"
#include "storage/ata.h"
#include "fs/vfs/mount.h"
#include "fs/ramfs/ramfs.h"
#include "memory/pmm.h"
#include "paging.h"
#include "user/user.h"
#include "mem/kmalloc.h"
#include "panic_sys.h"
#include "panic.h"
#include "sched/scheduler.h"
#include "sched/pcb.h"
#include "detect/ram.h"
#include "detect/tpm.h"


// ===== TASK SUBSYSTEM FALLBACK (in-file, no external headers) =====
//
// This block provides a *safe* in-file fallback implementation of the minimal
// task API used by kernel_main. Symbols are marked weak so a full task
// implementation (in kernel/task/*.o) will override them at link time.
//
// The fallback purpose:
//  - avoid compiler errors due to missing headers/signatures
//  - keep TASKS disabled by default so we don't attempt context switching
//    without a verified arch switch.S.

#define KSTACK_SIZE 8192

#ifdef __cplusplus
extern "C" {
#endif

typedef struct task {
    int pid;
    uint32_t *kstack_ptr;
    uint32_t cr3;
    uint8_t kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    struct task *next;
} task_t;

// Weak stubs: if you have a real implementation in task/*.o these get replaced.
__attribute__((weak)) void task_init(void) { /* fallback: no-op */ }
__attribute__((weak)) void task_init_scheduler(void) { task_init(); }

// Old API used by kernel.cpp: task_create(const char* name, void(*entry))
__attribute__((weak)) task_t *task_create(const char * /*name*/, void (*entry)(void)) {
    (void)entry; /* fallback does not create real tasks */
    return (task_t*)0;
}

// yield / aliases
__attribute__((weak)) void task_yield(void) { /* fallback: no-op */ }
// also provide yield() symbol (sometimes used)
__attribute__((weak)) void yield(void) { task_yield(); }

#ifdef __cplusplus
} // extern "C"
#endif

// ===== end fallback =====

// ===== other forward declarations / externs =====
/* If shell.h doesn't declare shell_poll_input(), provide a local fallback */
#ifdef __cplusplus
extern "C" {
#endif
void shell_poll_input(void);
#ifdef __cplusplus
}
#endif

extern "C" {
    void kmalloc_init(void);
}

/* Buddy allocator wrapper that hooks the buddy allocator to the heap region
   defined by the linker script (linker.ld) */
extern "C" void buddy_init_from_heap(void);

/* Classic Multiboot 1 magic constant */
#define MULTIBOOT_MAGIC 0x2BADB002u

/* Debug/test flag: set to 1 to exercise framebuffer logic (if available) */
#define VGA_TEST 0

/* IMPORTANT: by default we KEEP TASKING DISABLED to avoid bootloops.
   Set to 1 only after you're sure switch_to is correct and you have tested
   context switching in a controlled environment.
*/
#define TASKS_ENABLED 0

/* Minimal Multiboot structure (only the fields we use) */
typedef struct {
    uint32_t flags;
    /* many fields omitted intentionally */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
} multiboot_info_t;

/* Try to initialize the framebuffer if GRUB provided a framebuffer info block.
   This is minimal and defensive: it only touches VGA subsystem if the multiboot
   info reports a valid framebuffer. */
static void try_init_framebuffer_from_multiboot(uint32_t magic, uint32_t addr)
{
    if (magic != MULTIBOOT_MAGIC) return;
    if (addr == 0) return;

    multiboot_info_t* mbi = (multiboot_info_t*)(uintptr_t)addr;

    /* Multiboot spec: bit 12 = framebuffer info present */
    if (mbi->flags & (1 << 12)) {
        void* fb_addr = (void*)(uintptr_t)mbi->framebuffer_addr;
        uint32_t fb_width = mbi->framebuffer_width;
        uint32_t fb_height = mbi->framebuffer_height;
        uint32_t fb_pitch = mbi->framebuffer_pitch;
        uint8_t  fb_bpp = mbi->framebuffer_bpp;

        if (fb_addr && fb_width > 0 && fb_height > 0 && fb_pitch > 0 && fb_bpp > 0) {
            vga_set_framebuffer(fb_addr, fb_width, fb_height, fb_pitch, fb_bpp);
        }
    }
}

// -----------------------------
// Example tasks (cooperative)
// -----------------------------
// These demonstrate the cooperative API: tasks MUST call task_yield() so the
// scheduler can switch. If a task never yields, it will monopolize the CPU.
//
// NOTE: With TASKS_ENABLED==0 these are present but not started.
void task_a(void)
{
    for (;;) {
        terminal_writestring("[taskA] Task A running\n");
        for (volatile int i = 0; i < 1000000; ++i) ;
        task_yield(); /* yield to scheduler */
    }
}

void task_b(void)
{
    for (;;) {
        terminal_writestring("[taskB] Task B running\n");
        for (volatile int i = 0; i < 1000000; ++i) ;
        task_yield();
    }
}

// -----------------------------
// Helper: panic on kernel-detected fatal error
// -----------------------------
// Use this function for early fatal errors so the kernel shows the panic screen
// instead of continuing in a corrupted state. Note: this cannot catch hardware
// triple-faults that already reset the CPU.
static void panic_if_fatal(const char *msg)
{
    // try to print something sensible to both terminal and serial
    if (msg) {
        terminal_writestring("[PANIC] ");
        terminal_writestring(msg);
        terminal_writestring("\n");
        serial_write_string("[PANIC] ");
        serial_write_string(msg);
        serial_write_string("\r\n");
    }
    // register a minimal panic state and halt (panic() should not return)
    panic("Kernel detected fatal error; halting.");
}

// -----------------------------
// kernel_main - entry point
// -----------------------------
extern "C" void kernel_main(uint32_t magic, uint32_t addr) {

    // 1) Early: pick up framebuffer info (if GRUB gave it)
    try_init_framebuffer_from_multiboot(magic, addr);

    // 2) Terminal (text-mode) initialization - early so we can show errors
    terminal_init();

    // 3) Early boot safety check: detect RAM and panic cleanly if insufficient
    ram_check_or_panic(magic, addr);
    tpm_check_or_panic();

    // 4) CPU/interrupt basic setup: GDT -> IDT -> PIC. Order matters.
    gdt_init();
    idt_init();
    pic_remap();

    // 5) Optional boot logo
    bootlogo_show();

    // 6) Filesystem core: initialize and mount a minimal root
    fs_init();
    if (!ramfs_root()) {
        // ramfs_root failure is fatal for our simple early boot; panic instead of continuing
        panic_if_fatal("ramfs_root() returned NULL");
    }
    vfs_mount("/", ramfs_root());

    vnode_t* v = vfs_resolve("/");
    if (v)
        terminal_writestring("[vfs] root mounted OK\n");
    else
        terminal_writestring("[vfs] mount FAILED\n");

    // 7) User subsystem (if present)
    user_init();

    // 8) Shell: text UI
    shell_init();

    // 9) Physical Memory Manager (PMM)
    pmm_init((void*)addr);

    // Sanity check PMM (if you have functions for total frames, check them here)

    // 10) Input drivers: keyboard buffer + raw keyboard driver
    kbd_buffer_init();
    keyboard_init();

    /* === NEW === */
    /* Use task_init() which is a safe weak symbol fallback (no-op if no task impl). */
    task_init();

    // 11) Timer abstraction (PIT) - install handlers but keep interrupts disabled
    timer_init(100);

    // 12) Other drivers and serial for debug/log
    audio_init();
    mouse_init();

    serial_init();
    serial_write_string("=== Chrysalis OS serial online ===\r\n");

    // 13) Event queue for event-driven design
    event_queue_init();

    // 14) Now enable interrupts: only do this after driver IRQ handlers are installed.
    // Enabling earlier risks races where an ISR runs before supporting state is ready.
    asm volatile("sti");

    // 15) Demonstration of sleep / IRQ-driven timers working
    terminal_writestring("Sleeping 1 second...\n");
    sleep(1000);
    terminal_writestring("Done!\n");

    for (int i = 0; i < 5; i++) {
        terminal_writestring("tick\n");
        sleep(500);
    }

    terminal_writestring("\nSystem ready.\n> ");

    // 16) RTC/time/ATA
    rtc_print();
    time_init();
    time_set_timezone(2);
    datetime t;
    time_get_local(&t);
    ata_init();

    // 17) PMM quick test
    uint32_t frame1 = pmm_alloc_frame();
    uint32_t frame2 = pmm_alloc_frame();
    terminal_printf("PMM: %x %x\n", frame1, frame2);
    if (frame1 == 0 || frame2 == 0) {
        // allocation failure — panic instead of continuing in degraded state
        panic_if_fatal("pmm_alloc_frame returned 0 (allocation failed)");
    }
    pmm_free_frame(frame1);
    pmm_free_frame(frame2);

    // 18) Paging: choose page area size per your roadmap
    paging_init(PAGING_120_MB);
    terminal_printf("Paging OK\n");

    // 19) Heap + buddy allocator + kmalloc
    extern uint8_t __heap_start;
    extern uint8_t __heap_end;

    // defensive check: heap range must make sense
    if (&__heap_end <= &__heap_start) {
        panic_if_fatal("Heap region invalid (heap_end <= heap_start)");
    }

    heap_init(&__heap_start, (size_t)(&__heap_end - &__heap_start));
    buddy_init_from_heap();
    kmalloc_init();

    // 20) Heap test (defensive frees)
    void* heap_a = kmalloc(64);
    void* heap_b = kmalloc_aligned(100, 64);
    if (!heap_a || !heap_b) {
        // kmalloc failure early indicates severe problem: panic
        panic_if_fatal("kmalloc/kmalloc_aligned failed during early heap test");
    }
    kfree(heap_a);
    kfree(heap_b);

    // 21) Register system info for panic screen (use safe defaults)
    uint32_t total_kb = 0;
    uint32_t free_kb  = 0;
    uint32_t uptime_s = 0;

    if (magic == MULTIBOOT_MAGIC && addr != 0) {
        struct mb_basic {
            uint32_t flags;
            uint32_t mem_lower;
            uint32_t mem_upper;
        };
        struct mb_basic* mb = (struct mb_basic*)(uintptr_t)addr;
        if (mb && (mb->flags & 0x1)) {
            total_kb = mb->mem_lower + mb->mem_upper;
        }
    }

    const char *cpu_detected = panic_sys_cpu_str();
    panic_sys_register_memory_kb(total_kb, free_kb);
    panic_sys_register_storage_kb(0, 0); // populate if you have storage info
    panic_sys_register_cpu_str(cpu_detected);
    panic_sys_register_uptime_seconds(uptime_s);

    // -----------------------------
    // TASKS: Setup cooperative scheduling (DEFENSIVE)
    // -----------------------------
#if TASKS_ENABLED
    terminal_writestring("[sched] tasks created; scheduler disabled by default\n");
    terminal_writestring("[sched] call scheduler_start() manually when ready\n");

    /* Initialize scheduler structures but DO NOT start scheduling automatically.
       This prevents an immediate context switch into untested stacks. */
    scheduler_init(2);

    /* Create tasks (we log failures but do NOT panic automatically). */
    int r;
    r = pcb_create(task_a, NULL);
    if (r < 0) {
        terminal_printf("[sched] pcb_create(taskA) -> %d\n", r);
        serial_write_string("[sched] pcb_create(taskA) failed\r\n");
    }

    r = pcb_create(task_b, NULL);
    if (r < 0) {
        terminal_printf("[sched] pcb_create(taskB) -> %d\n", r);
        serial_write_string("[sched] pcb_create(taskB) failed\r\n");
    }

    terminal_writestring("[sched] scheduler initialized and tasks created (scheduler NOT started)\n");
    terminal_writestring("[sched] To start scheduler manually call: scheduler_start();\n");
#else
    terminal_writestring("[sched] TASKS_ENABLED=0 (scheduler disabled)\n");
#endif



    // 22) Main loop: shell polling + halt (no unsafe yields)
    // If you want the shell to be preempted by tasks, move shell into its own
    // task and enable TASKS_ENABLED after implementing a safe switch_to.
    while (1) {
        shell_poll_input();   // non-blocking poll for shell input

#if TASKS_ENABLED
        // If you truly want automated scheduling when TASKS_ENABLED==1, replace
        // the following line with task_yield() once you verified switch_to and
        // have tested on a VM snapshot (so you can revert on triple fault).
        // task_yield();
#endif

        asm volatile("hlt"); // reduce power until next interrupt
    }
}

// =========================
// Notes for modders (concise checklist)
// =========================
//
// - To get real preemptive scheduling:
//
//   * Implement a robust arch/i386/switch.S that saves/restores full context
//     (pushad / push eflags / save cs/ss if necessary). The simple `movl %esp, (prev)`
//     + `movl (next), %esp` + `ret` technique is fragile for complex kernels.
//   * In the PIT IRQ handler, save the CPU context and call schedule().
//   * Add a safe trampoline for user-mode tasks (iret frame).
//
// - To avoid triple-faults and display panics instead:
//
//   * Install a double-fault handler in the IDT that calls a panic handler which
//     prints debug info and halts (or drops to a safe console). If a double-fault
//     handler is itself invalid, the CPU triple-faults and resets (not catchable).
//
// - Diagnostics tips:
//
//   * Keep serial outputs in early boot — easiest way to see what failed.
//   * When enabling TASKS_ENABLED, run inside a VM snapshot so you can revert
//     quickly if a triple-fault occurs.
//   * Create a small `switch_test` routine that exercises switching in a controlled
//     environment (no interrupts, tiny stacks), then expand to IRQ-driven scheduling.
//
// End of file

/* repaired linker.ld - simple, ld-friendly layout */
ENTRY(_start)

SECTIONS
{
    /* load address: 1MiB */
    . = 1M;

    .multiboot : { *(.multiboot) }

    .text : { *(.text*) }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }

    /* ================== KERNEL HEAP ================== */
    . = ALIGN(0x1000);
    __heap_start = .;
    . += 0x100000;          /* 1 MiB heap kernel */
    __heap_end = .;
    . = ALIGN(0x1000);
    /* ================================================= */

    /* === IMPORTANT: end of kernel image === */
    kernel_end = .;

    /DISCARD/ : { *(.eh_frame) }
}

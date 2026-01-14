ENTRY(_start)

SECTIONS
{
    /* load address: 1MiB */
    . = 1M;

    .text :
    {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    }

    /* ================== KERNEL HEAP ================== */
    . = ALIGN(0x1000);
    __heap_start = .;
    . += 0x2000000; /* 32MB Heap for GUI buffers */
    __heap_end = .;
    . = ALIGN(0x1000);
    /* ================================================= */

    kernel_end = .;

    /DISCARD/ : { *(.eh_frame) }
}

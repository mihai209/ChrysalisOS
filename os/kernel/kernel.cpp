extern "C" void gdt_init();
extern "C" void idt_init();

extern "C" void kernel_main()
{
    gdt_init();
    idt_init();

    // NU activa intreruperile inca
    // asm volatile("sti");

    while (1)
        asm volatile("hlt");
}

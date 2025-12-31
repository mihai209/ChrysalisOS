extern "C" void gdt_init();
extern "C" void idt_init();
extern "C" void pic_remap(int, int);

extern "C" void kernel_main()
{
    gdt_init();
    idt_init();
    pic_remap(32, 40);

    asm volatile("sti");

    while (1)
        asm volatile("hlt");
}

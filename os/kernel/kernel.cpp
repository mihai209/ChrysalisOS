extern "C" void idt_init();
extern "C" void pic_remap(int, int);

extern "C" void kernel_main() {
    idt_init();
    pic_remap(32, 40);

    asm volatile("sti");

    while (1)
        asm volatile("hlt");
}

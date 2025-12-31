extern "C" void gdt_init();

extern "C" void kernel_main() {
    gdt_init();

    // deocamdată NU activăm întreruperi
    // asm volatile("sti");

    while (1)
        asm volatile("hlt");
}

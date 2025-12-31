extern "C" void isr_handler()
{
    while (1) {
        asm volatile("hlt");
    }
}

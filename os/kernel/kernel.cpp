extern "C" void kernel_main() {
    const char* msg = "Hello from Chrysalis OS!";
    char* video = (char*)0xB8000;

    for (int i = 0; msg[i] != 0; i++) {
        video[i * 2] = msg[i];
        video[i * 2 + 1] = 0x0F;
    }

    while (true) {
        asm volatile("hlt");
    }
}

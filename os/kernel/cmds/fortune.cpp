#include "../terminal.h"
#include "../drivers/pit.h"

static const char* fortunes[] = {
    "Code is law.",
    "One bug at a time.",
    "It boots? Ship it.",
    "Undefined behavior is still behavior.",
    "Kernel dev chooses pain."
};

extern "C" void cmd_fortune(const char* args)
{
    (void)args;

    uint32_t t = pit_get_ticks();
    uint32_t idx = t % (sizeof(fortunes) / sizeof(fortunes[0]));

    terminal_writestring(fortunes[idx]);
    terminal_writestring("\n");
}

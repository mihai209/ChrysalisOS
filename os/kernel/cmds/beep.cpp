#include "../drivers/speaker.h"
#include "../drivers/shortcuts.h"
#include "../terminal.h"

extern "C" void cmd_beep(const char* args)
{
    (void)args;

    terminal_writestring("Beep! (Ctrl+C to stop)\n");

    shortcut_ctrl_c = false;

    speaker_on(440);

    while (!shortcut_ctrl_c) {
        asm volatile("hlt");
    }

    speaker_stop();
    terminal_writestring("^C\n");
}

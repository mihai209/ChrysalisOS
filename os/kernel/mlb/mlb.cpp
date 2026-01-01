#include "mlb.h"
#include "../terminal.h"
#include "../drivers/speaker.h"

static int playing = 0;
static const char* current = nullptr;

void mlb_print_catalog()
{
    terminal_writestring("Music library:\n");
    terminal_writestring("  1 - beep440\n");
    terminal_writestring("  2 - beep880\n");
}

void mlb_play(const char* id)
{
    if (!id || !id[0]) {
        terminal_writestring("play: missing song id\n");
        return;
    }

    if (playing) {
        terminal_writestring("Already playing. Use play --stop\n");
        return;
    }

    if (id[0] == '1') {
        speaker_on(440);
        current = "beep440";
    }
    else if (id[0] == '2') {
        speaker_on(880);
        current = "beep880";
    }
    else {
        terminal_writestring("Unknown song id\n");
        return;
    }

    playing = 1;
    terminal_writestring("Playing: ");
    terminal_writestring(current);
    terminal_writestring("\n");
}

void mlb_stop()
{
    if (!playing) {
        terminal_writestring("Nothing is playing\n");
        return;
    }

    speaker_stop();
    playing = 0;
    current = nullptr;

    terminal_writestring("Playback stopped\n");
}

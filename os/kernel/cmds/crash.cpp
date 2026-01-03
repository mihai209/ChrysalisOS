#include "../terminal.h"
#include "../panic.h"

extern "C" void cmd_crash(const char* args)
{
    if (!args || !*args) {
        /* mesaj implicit când nu există argumente */
        panic("Manual crash triggered (no additional message).");
        return; /* panic() normally doesn't return, but keep return for safety */
    }

    /* forward raw args string to panic so user message appears verbatim */
    panic(args);
}

#include "shell.h"
#include "../terminal.h"
#include "../cmds/registry.h"
#include "../input/keyboard_buffer.h"

/* event queue includes */
#include "../events/event.h"
#include "../events/event_queue.h"

static char buffer[128];
static int index = 0;

/* mini strcmp 
static int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}*/

static void execute_command(const char* input) {
    for (int i = 0; i < command_count; i++) {
        const char* name = command_table[i].name;

        int j = 0;
        while (name[j] && input[j] && name[j] == input[j])
            j++;

        /* potrivire exactă sau cu argument */
        if (name[j] == 0 && (input[j] == 0 || input[j] == ' ')) {
            const char* args = (input[j] == ' ') ? input + j + 1 : "";
            command_table[i].func(args);
            return;
        }
    }

    terminal_writestring("Unknown command\n");
}

void shell_init() {
    index = 0;
    terminal_writestring("> ");
}

void shell_handle_char(char c) {
    if (c == '\n') {
        buffer[index] = 0;
        terminal_putchar('\n');

        execute_command(buffer);

        terminal_writestring("> ");
        index = 0;
        return;
    }

    if (c == '\b') {
        if (index > 0) {
            index--;
            terminal_putchar('\b');
        }
        return;
    }

    if (index < 127) {
        buffer[index++] = c;
        terminal_putchar(c);
    }
}

/* --- new: poll input from event_queue first, then fallback to keyboard_buffer --- */
void shell_poll_input(void)
{
    /* 1) consume events from event queue (preferred) */
    event_t ev;
    while (event_pop(&ev) == 0) {
        if (ev.type == EVENT_KEY) {
            /* only feed on key press events (pressed == 1) to match previous behavior */
            if (ev.key.pressed) {
                /* if ascii==0 (non-printable), we ignore — you can extend this later */
                if (ev.key.ascii) {
                    shell_handle_char(ev.key.ascii);
                }
            }
        }
        /* future: handle EVENT_MOUSE, EVENT_TIMER, etc. */
    }

    /* 2) fallback: consume legacy keyboard buffer (compat) 
    while (kbd_has_char()) {
        char c = kbd_get_char();
        shell_handle_char(c);
    }*/
}

void shell_reset_input(void) {
    index = 0;
}

void shell_prompt(void) {
    terminal_writestring("> ");
}

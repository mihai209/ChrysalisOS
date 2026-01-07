#include "shell.h"
#include "../terminal.h"
#include "../cmds/registry.h"
#include "../input/keyboard_buffer.h"

/* event queue includes */
#include "../events/event.h"
#include "../events/event_queue.h"

/* user support for prompt */
#include "../user/user.h"

static char buffer[128];
static int index = 0;

/* mini strcmp (fără libc) */
static int str_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void execute_command(const char* input) {
    /* copiem input într-un buffer modificabil */
    char tmp[128];
    int i = 0;
    while (input[i] && i < (int)sizeof(tmp) - 1) { tmp[i] = input[i]; i++; }
    tmp[i] = '\0';

    /* tokenizare simplă pe spațiu */
    char *argv[16];
    int argc = 0;
    char *p = tmp;

    /* skip leading spaces */
    while (*p == ' ') p++;

    while (*p && argc < (int)(sizeof(argv)/sizeof(argv[0]))) {
        argv[argc++] = p;
        /* avansăm la următorul spațiu */
        while (*p && *p != ' ') p++;
        if (*p == ' ') {
            *p = '\0';
            p++;
            while (*p == ' ') p++; /* skip multiple spaces */
        }
    }

    if (argc == 0) return; /* linie goală */

    /* caută comanda în tabel și apelează-o */
    for (int j = 0; j < command_count; ++j) {
        if (str_eq(argv[0], command_table[j].name)) {
            /* apelăm funcția comenzii */
            command_table[j].func(argc, argv);
            return;
        }
    }

    terminal_writestring("Unknown command\n");
}

void shell_init() {
    index = 0;
    shell_prompt();
}

void shell_handle_char(char c) {
    if (c == '\n') {
        buffer[index] = 0;
        terminal_putchar('\n');

        execute_command(buffer);

        shell_prompt();
        index = 0;
        return;
    }

    if (c == '\b') {
        if (index > 0) {
            index--;
            /* backspace visual (simple) */
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
                /* if ascii==0 (non-printable), we ignore */
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
    user_t* u = user_get_current();
    if (u) {
        terminal_printf("%s@chrysalis:~$ ", u->name);
    } else {
        terminal_printf("guest@chrysalis:~$ ");
    }
}

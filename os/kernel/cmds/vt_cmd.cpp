#include "vt_cmd.h"
#include "../vt/vt.h"
#include "../terminal.h"
#include "../string.h"

/* Simple atoi helper since we don't have a standard library one exposed globally yet */
static int k_atoi(const char* s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

static void print_help() {
    terminal_writestring("Usage: vt [command] [args]\n\n");
    terminal_writestring("Virtual Terminals control utility.\n\n");
    terminal_writestring("Commands:\n");
    terminal_writestring("  vt list            List all virtual terminals\n");
    terminal_writestring("  vt switch <id>     Switch to VT <id>\n");
    terminal_writestring("  vt active          Show active VT\n");
    terminal_writestring("  vt bind <id> <fn>  Bind function to VT (shell, log, debug)\n");
    terminal_writestring("  vt clear <id>      Clear VT screen\n");
    terminal_writestring("  vt help            Show this help\n\n");
    terminal_writestring("Notes:\n");
    terminal_writestring("  - VT switching is also available via Ctrl+Alt+F1..F8\n");
    terminal_writestring("  - VT 0 is the main user shell\n");
    terminal_writestring("  - VT 1 is reserved for kernel logs\n");
    terminal_writestring("  - Serial output is NOT affected by VT\n");
}

extern "C" int cmd_vt(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    const char* sub = argv[1];

    if (strcmp(sub, "help") == 0 || strcmp(sub, "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(sub, "list") == 0) {
        int active = vt_active();
        for (int i = 0; i < VT_COUNT; i++) {
            const char* role = vt_get_role(i);
            terminal_printf("VT%d  %-10s %s\n", i, role, (i == active) ? "active" : "");
        }
        return 0;
    }

    if (strcmp(sub, "active") == 0) {
        int active = vt_active();
        const char* role = vt_get_role(active);
        terminal_printf("Active VT: %d (%s)\n", active, role);
        return 0;
    }

    if (strcmp(sub, "switch") == 0) {
        if (argc < 3) {
            terminal_writestring("Usage: vt switch <id>\n");
            return -1;
        }
        int id = k_atoi(argv[2]);
        if (id < 0 || id >= VT_COUNT) {
            terminal_printf("Invalid VT ID (0-%d)\n", VT_COUNT - 1);
            return -1;
        }
        vt_switch(id);
        const char* role = vt_get_role(id);
        terminal_printf("Switched to VT%d (%s)\n", id, role);
        return 0;
    }

    if (strcmp(sub, "bind") == 0) {
        if (argc < 4) {
            terminal_writestring("Usage: vt bind <id> <role>\n");
            return -1;
        }
        int id = k_atoi(argv[2]);
        const char* role = argv[3];
        if (id < 0 || id >= VT_COUNT) {
            terminal_printf("Invalid VT ID (0-%d)\n", VT_COUNT - 1);
            return -1;
        }
        vt_set_role(id, role);
        terminal_printf("VT%d bound to '%s'\n", id, role);
        return 0;
    }

    if (strcmp(sub, "clear") == 0) {
        if (argc < 3) {
            terminal_writestring("Usage: vt clear <id>\n");
            return -1;
        }
        int id = k_atoi(argv[2]);
        vt_clear(id);
        return 0;
    }

    print_help();
    return 0;
}
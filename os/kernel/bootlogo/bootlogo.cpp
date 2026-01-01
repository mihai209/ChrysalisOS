#include "bootlogo.h"
#include "../terminal.h"

static void print_centered(const char* text, int row) {
    int len = 0;
    while (text[len]) len++;

    int col = (80 - len) / 2;

    for (int i = 0; i < col; i++)
        terminal_putchar(' ');

    terminal_writestring(text);
}

void bootlogo_show() {
    terminal_clear();

    print_centered("#############################", 0);
    print_centered("#                           #", 1);
    print_centered("#     CHRYSALIS  OS         #", 2);
    print_centered("#                           #", 3);
    print_centered("#############################", 4);

    terminal_writestring("\n\nInitializing system...\n");
}

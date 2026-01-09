/* kernel/cmds/write.cpp
 * Minimal interactive line-based text editor.
 */

#include "write.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../input/input.h"
#include "fat.h"

/* FAT32 Driver API (External) */
extern "C" int fat32_create_file(const char* path, const void* data, uint32_t size);
extern "C" int fat32_read_file(const char* path, void* buf, uint32_t max_size);

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

#define MAX_FILE_SIZE (64 * 1024)
#define KEY_CTRL_C    3
#define KEY_CTRL_D    4
#define KEY_CTRL_S    19
#define KEY_BACKSPACE '\b'
#define KEY_ENTER     '\n'
#define KEY_RETURN    '\r'

extern "C" int cmd_write(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Usage: write <path>\n");
        return -1;
    }

    const char* path = argv[1];

    /* 1. Automount FAT32 */
    fat_automount();

    /* 2. Allocate Buffer */
    char* buffer = (char*)kmalloc(MAX_FILE_SIZE);
    if (!buffer) {
        terminal_writestring("Error: Out of memory (64KB limit)\n");
        return -1;
    }

    uint32_t current_size = 0;

    /* 3. Load existing file content */
    int bytes_read = fat32_read_file(path, buffer, MAX_FILE_SIZE);
    if (bytes_read > 0) {
        current_size = (uint32_t)bytes_read;
        terminal_printf("Loaded %d bytes from '%s'.\n", current_size, path);
    } else {
        terminal_printf("New file: '%s'\n", path);
    }

    /* 4. UI Header */
    terminal_writestring("Chrysalis Line Editor\n");
    terminal_writestring("Type text. ENTER for new line.\n");
    terminal_writestring("Ctrl+S or Ctrl+D to Save & Exit | Ctrl+C to Abort\n");
    terminal_writestring("---------------------------------------\n");

    serial("[WRITE] Editor started for %s\n", path);

    /* 5. Input Loop */
    input_event_t ev;
    bool running = true;
    bool save = false;

    while (running) {
        if (input_pop(&ev)) {
            if (ev.type == INPUT_KEYBOARD && ev.pressed) {
                char c = (char)ev.keycode;

                if (c == KEY_CTRL_C) {
                    terminal_writestring("^C\nAborted.\n");
                    serial("[WRITE] Aborted by user.\n");
                    running = false;
                    save = false;
                }
                else if (c == KEY_CTRL_D || c == KEY_CTRL_S) {
                    terminal_writestring(c == KEY_CTRL_D ? "^D\n" : "^S\n");
                    running = false;
                    save = true;
                }
                else if (c == KEY_BACKSPACE) {
                    if (current_size > 0) {
                        /* Simple backspace: remove last char from buffer and screen */
                        current_size--;
                        terminal_putchar('\b');
                        terminal_putchar(' ');
                        terminal_putchar('\b');
                    }
                }
                else if (c == KEY_RETURN || c == KEY_ENTER) {
                    if (current_size < MAX_FILE_SIZE - 1) {
                        buffer[current_size++] = '\n';
                        terminal_putchar('\n');
                    }
                }
                else if (c >= 32 && c <= 126) { /* Printable characters */
                    if (current_size < MAX_FILE_SIZE - 1) {
                        buffer[current_size++] = c;
                        terminal_putchar(c);
                    }
                }
            }
        } else {
            /* Wait for interrupt to avoid burning CPU */
            asm volatile("hlt");
        }
    }

    /* 6. Save or Discard */
    int ret = 0;
    if (save) {
        terminal_writestring("Saving... ");
        int r = fat32_create_file(path, buffer, current_size);
        if (r == 0) {
            terminal_writestring("OK.\n");
            serial("[WRITE] Saved %d bytes to %s\n", current_size, path);
        } else {
            terminal_writestring("FAILED.\n");
            serial("[WRITE] Save failed for %s\n", path);
            ret = -1;
        }
    }

    kfree(buffer);
    return ret;
}
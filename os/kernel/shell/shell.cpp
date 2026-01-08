#include "shell.h"
#include "../terminal.h"
#include "../cmds/registry.h"
#include "../drivers/serial.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../user/user.h"
#include "../events/event_queue.h"
#include "../proc/exec.h"

/* Configuration */
#define SHELL_BUF_SIZE 256
#define SHELL_MAX_ARGS 32
#define HISTORY_SIZE 16
#define PIPE_BUF_SIZE 4096

/* Key definitions (ASCII control codes) */
#define KEY_ENTER       '\n'
#define KEY_BACKSPACE   '\b'
#define KEY_TAB         '\t'
#define KEY_CTRL_P      16  /* Up (Previous) */
#define KEY_CTRL_N      14  /* Down (Next) */

/* State */
static char line[SHELL_BUF_SIZE];
static int line_len = 0;
static int cursor = 0;
static int last_rendered_len = 0;

static char history[HISTORY_SIZE][SHELL_BUF_SIZE];
static int hist_head = 0; // Next write slot
static int hist_count = 0;
static int hist_pos = -1; // -1 = current editing, 0..count-1 = history index

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

/* --- Helpers --- */

static void shell_render_line() {
    /* Move to start of line */
    terminal_putchar('\r');
    
    /* Render prompt */
    shell_prompt();
    
    /* Render buffer */
    for (int i = 0; i < line_len; i++) {
        terminal_putchar(line[i]);
    }
    
    /* Clear trailing garbage if line shrank */
    if (last_rendered_len > line_len) {
        for (int i = 0; i < (last_rendered_len - line_len); i++) terminal_putchar(' ');
        for (int i = 0; i < (last_rendered_len - line_len); i++) terminal_putchar('\b');
    }
    last_rendered_len = line_len;
    
    /* Move cursor visually to correct position */
    for (int i = line_len; i > cursor; i--) {
        terminal_putchar('\b');
    }
}

static void shell_history_add(const char* cmd) {
    if (!cmd || !*cmd) return;
    
    /* Don't add duplicates of the immediate last command */
    if (hist_count > 0) {
        int last_idx = (hist_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(history[last_idx], cmd) == 0) return;
    }
    
    /* Add to ring buffer */
    strcpy(history[hist_head], cmd);
    hist_head = (hist_head + 1) % HISTORY_SIZE;
    if (hist_count < HISTORY_SIZE) hist_count++;
    
    serial("[SHELL] History add: %s\n", cmd);
}

static void shell_history_nav(int dir) {
    /* dir: 1 for UP (older), -1 for DOWN (newer) */
    if (hist_count == 0) return;
    
    if (hist_pos == -1) {
        /* Currently editing new line */
        if (dir == 1) {
            hist_pos = 0; /* Go to latest history */
        } else {
            return;
        }
    } else {
        hist_pos += dir;
    }
    
    /* Bounds check */
    if (hist_pos < -1) hist_pos = -1;
    if (hist_pos >= hist_count) hist_pos = hist_count - 1;
    
    /* Load line */
    if (hist_pos == -1) {
        line[0] = 0;
        line_len = 0;
        cursor = 0;
    } else {
        /* Calculate actual index in ring buffer (0 = latest) */
        int idx = (hist_head - 1 - hist_pos + HISTORY_SIZE * 2) % HISTORY_SIZE;
        strcpy(line, history[idx]);
        line_len = strlen(line);
        cursor = line_len;
    }
    
    shell_render_line();
    serial("[SHELL] History nav: pos=%d\n", hist_pos);
}

static void shell_exec_single(char* cmd_str) {
    /* Parse arguments */
    char* argv[SHELL_MAX_ARGS];
    int argc = 0;
    
    char* p = cmd_str;
    while (*p && argc < SHELL_MAX_ARGS) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        /* Start of token */
        if (*p == '"') {
            /* Quoted string */
            p++; // Skip opening quote
            argv[argc++] = p;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) {
                    /* Handle escape: skip backslash, keep next char */
                    /* In-place shift would be better, but for now we just skip \ */
                    p++; 
                }
                p++;
            }
            if (*p == '"') *p++ = 0; // Terminate and skip closing quote
        } else {
            /* Normal word */
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = 0; // Terminate
        }
    }
    
    if (argc > 0) {
        serial("[SHELL] Exec: argc=%d cmd='%s'\n", argc, argv[0]);

        /* Check for Chrysalis Script (.csr / .chs) */
        int len = strlen(argv[0]);
        if (len > 4) {
            const char* ext = argv[0] + len - 4;
            if (strcmp(ext, ".csr") == 0 || strcmp(ext, ".chs") == 0) {
                serial("[SHELL] Detected script: %s\n", argv[0]);
                
                /* Construct argv for exec: ["cs", script_path, args...] */
                char* new_argv[SHELL_MAX_ARGS + 2];
                new_argv[0] = (char*)"cs";
                new_argv[1] = argv[0]; /* Script path */
                
                for (int i = 1; i < argc; i++) {
                    new_argv[i + 1] = argv[i];
                }
                new_argv[argc + 1] = 0; /* Null terminate */

                /* Launch interpreter via exec */
                /* This maps to /bin/cs inside execve */
                execve("/bin/cs", new_argv, 0);
                return;
            }
        }
        
        /* Lookup command */
        int found = 0;
        for (int i = 0; i < command_count; i++) {
            if (strcmp(command_table[i].name, argv[0]) == 0) {
                command_table[i].func(argc, argv);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            terminal_writestring("Unknown command: ");
            terminal_writestring(argv[0]);
            terminal_writestring("\n");
        }
    }
}

static void shell_exec_line() {
    terminal_putchar('\n');
    
    if (line_len == 0) {
        shell_prompt();
        return;
    }
    
    /* Add to history */
    shell_history_add(line);
    hist_pos = -1; // Reset history navigation

    /* Pipe buffers */
    char* buf_in = 0;
    size_t len_in = 0;
    
    char* buf_out = 0;
    size_t len_out = 0;

    /* Split by pipe '|' */
    char* p = line;
    char* cmd_start = p;
    
    while (*p) {
        /* Find next pipe or end of string */
        char* pipe_ptr = p;
        while (*pipe_ptr && *pipe_ptr != '|') pipe_ptr++;
        
        bool is_last = (*pipe_ptr == 0);
        if (!is_last) {
            *pipe_ptr = 0; /* Terminate current command string */
        }

        /* Prepare Output Buffer if not last */
        if (!is_last) {
            buf_out = (char*)kmalloc(PIPE_BUF_SIZE);
            if (buf_out) {
                serial("[SHELL] Pipe created (buf=0x%x)\n", buf_out);
                terminal_start_capture(buf_out, PIPE_BUF_SIZE, &len_out);
            } else {
                serial("[SHELL] Pipe alloc failed!\n");
            }
        } else {
            terminal_end_capture();
        }

        /* Setup Input from previous pipe */
        if (buf_in) {
            terminal_set_input(buf_in, len_in);
        } else {
            terminal_set_input(0, 0);
        }

        /* Execute Command */
        shell_exec_single(cmd_start);

        /* Cleanup Input */
        if (buf_in) {
            kfree(buf_in);
            buf_in = 0;
            len_in = 0;
        }

        /* Prepare for next command */
        if (!is_last) {
            terminal_end_capture();
            if (buf_out) {
                serial("[SHELL] Pipe write: %d bytes\n", len_out);
                buf_in = buf_out; /* Output becomes input for next */
                len_in = len_out;
                buf_out = 0;
            }
            cmd_start = pipe_ptr + 1;
            p = cmd_start;
        } else {
            break;
        }
    }
    
    /* Ensure cleanup */
    terminal_set_input(0, 0);
    terminal_end_capture();
    if (buf_in) kfree(buf_in);
    if (buf_out) kfree(buf_out);
    
    /* Reset line */
    line[0] = 0;
    line_len = 0;
    cursor = 0;
    last_rendered_len = 0;
    
    shell_prompt();
}

static void shell_autocomplete() {
    /* Find word start */
    int start = cursor;
    while (start > 0 && line[start-1] != ' ') start--;
    
    int word_len = cursor - start;
    if (word_len == 0) return;
    
    char prefix[64];
    int i;
    for(i=0; i<word_len && i<63; i++) prefix[i] = line[start+i];
    prefix[i] = 0;
    
    serial("[SHELL] Autocomplete prefix: '%s'\n", prefix);
    
    int matches = 0;
    const char* last_match = 0;
    
    for (int j = 0; j < command_count; j++) {
        if (strncmp(command_table[j].name, prefix, word_len) == 0) {
            matches++;
            last_match = command_table[j].name;
        }
    }
    
    if (matches == 1) {
        /* Complete it */
        const char* rest = last_match + word_len;
        while (*rest) {
            if (line_len < SHELL_BUF_SIZE - 1) {
                line[line_len++] = *rest;
                line[line_len] = 0;
                cursor++;
            }
            rest++;
        }
        /* Add space */
        if (line_len < SHELL_BUF_SIZE - 1) {
            line[line_len++] = ' ';
            line[line_len] = 0;
            cursor++;
        }
        shell_render_line();
    } else if (matches > 1) {
        /* Show candidates */
        terminal_putchar('\n');
        for (int j = 0; j < command_count; j++) {
            if (strncmp(command_table[j].name, prefix, word_len) == 0) {
                terminal_writestring(command_table[j].name);
                terminal_writestring(" ");
            }
        }
        terminal_putchar('\n');
        shell_render_line();
    }
}

/* --- Public API --- */

void shell_init() {
    line[0] = 0;
    line_len = 0;
    cursor = 0;
    hist_head = 0;
    hist_count = 0;
    hist_pos = -1;
    
    serial("[SHELL] Initialized.\n");
    shell_prompt();
}

void shell_handle_char(char c) {
    if (c == KEY_ENTER) {
        shell_exec_line();
        return;
    }
    
    if (c == KEY_BACKSPACE) {
        if (cursor > 0) {
            /* Shift left */
            for (int i = cursor; i < line_len; i++) {
                line[i-1] = line[i];
            }
            line_len--;
            cursor--;
            line[line_len] = 0;
            shell_render_line();
        }
        return;
    }
    
    if (c == KEY_TAB) {
        shell_autocomplete();
        return;
    }
    
    /* History Navigation (Ctrl-P/N as fallback for Up/Down) */
    if (c == KEY_CTRL_P) {
        shell_history_nav(1);
        return;
    }
    if (c == KEY_CTRL_N) {
        shell_history_nav(-1);
        return;
    }
    
    /* Printable characters */
    if (c >= 32 && c <= 126) {
        if (line_len < SHELL_BUF_SIZE - 1) {
            /* Insert at cursor */
            for (int i = line_len; i > cursor; i--) {
                line[i] = line[i-1];
            }
            line[cursor] = c;
            line_len++;
            cursor++;
            line[line_len] = 0;
            shell_render_line();
        }
    }
}

void shell_reset_input(void) {
    line[0] = 0;
    line_len = 0;
    cursor = 0;
}

void shell_prompt(void) {
    user_t* u = user_get_current();
    if (u) {
        terminal_printf("%s@chrysalis:~$ ", u->name);
    } else {
        terminal_printf("guest@chrysalis:~$ ");
    }
}

/* Legacy poll support */
void shell_poll_input() {
    event_t ev;
    while (event_pop(&ev) == 0) {
        if (ev.type == EVENT_KEY && ev.key.pressed && ev.key.ascii) {
            shell_handle_char(ev.key.ascii);
        }
    }
}

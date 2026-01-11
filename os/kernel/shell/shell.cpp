#include "shell.h"
#include "../terminal.h"
#include "../cmds/registry.h"
#include "../drivers/serial.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../user/user.h"
#include "../events/event_queue.h"
#include "../proc/exec.h"
#include "../cmds/cd.h"
#include "../ui/wm/wm.h"
#include "../video/surface.h"
#include "../ui/flyui/draw.h"

/* Configuration */
#define SHELL_BUF_SIZE 256
#define SHELL_MAX_ARGS 32
#define HISTORY_SIZE 16
#define PIPE_BUF_SIZE 4096
#define MAX_SHELL_CONTEXTS 8

/* Key definitions (ASCII control codes) */
#define KEY_ENTER       '\n'
#define KEY_BACKSPACE   '\b'
#define KEY_TAB         '\t'
#define KEY_CTRL_P      16  /* Up (Previous) */
#define KEY_CTRL_N      14  /* Down (Next) */

/* Shell Context Structure */
typedef struct {
    char line[SHELL_BUF_SIZE];
    int line_len;
    int cursor;
    int last_rendered_len;
    char history[HISTORY_SIZE][SHELL_BUF_SIZE];
    int hist_head;
    int hist_count;
    int hist_pos;
} shell_ctx_t;

static shell_ctx_t contexts[MAX_SHELL_CONTEXTS];
static int active_ctx_id = 0;

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

/* --- Helpers --- */

static void shell_render_line() {
    shell_ctx_t* ctx = &contexts[active_ctx_id];

    /* Move to start of line */
    terminal_putchar('\r');
    
    /* Render prompt */
    shell_prompt();
    
    /* Render buffer */
    for (int i = 0; i < ctx->line_len; i++) {
        terminal_putchar(ctx->line[i]);
    }
    
    /* Clear trailing garbage if line shrank */
    if (ctx->last_rendered_len > ctx->line_len) {
        for (int i = 0; i < (ctx->last_rendered_len - ctx->line_len); i++) terminal_putchar(' ');
        for (int i = 0; i < (ctx->last_rendered_len - ctx->line_len); i++) terminal_putchar('\b');
    }
    ctx->last_rendered_len = ctx->line_len;
    
    /* Move cursor visually to correct position */
    for (int i = ctx->line_len; i > ctx->cursor; i--) {
        terminal_putchar('\b');
    }
}

static void shell_history_add(const char* cmd) {
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    if (!cmd || !*cmd) return;
    
    /* Don't add duplicates of the immediate last command */
    if (ctx->hist_count > 0) {
        int last_idx = (ctx->hist_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(ctx->history[last_idx], cmd) == 0) return;
    }
    
    /* Add to ring buffer */
    strcpy(ctx->history[ctx->hist_head], cmd);
    ctx->hist_head = (ctx->hist_head + 1) % HISTORY_SIZE;
    if (ctx->hist_count < HISTORY_SIZE) ctx->hist_count++;
    
    serial("[SHELL] History add: %s\n", cmd);
}

static void shell_history_nav(int dir) {
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    /* dir: 1 for UP (older), -1 for DOWN (newer) */
    if (ctx->hist_count == 0) return;
    
    if (ctx->hist_pos == -1) {
        /* Currently editing new line */
        if (dir == 1) {
            ctx->hist_pos = 0; /* Go to latest history */
        } else {
            return;
        }
    } else {
        ctx->hist_pos += dir;
    }
    
    /* Bounds check */
    if (ctx->hist_pos < -1) ctx->hist_pos = -1;
    if (ctx->hist_pos >= ctx->hist_count) ctx->hist_pos = ctx->hist_count - 1;
    
    /* Load line */
    if (ctx->hist_pos == -1) {
        ctx->line[0] = 0;
        ctx->line_len = 0;
        ctx->cursor = 0;
    } else {
        /* Calculate actual index in ring buffer (0 = latest) */
        int idx = (ctx->hist_head - 1 - ctx->hist_pos + HISTORY_SIZE * 2) % HISTORY_SIZE;
        strcpy(ctx->line, ctx->history[idx]);
        ctx->line_len = strlen(ctx->line);
        ctx->cursor = ctx->line_len;
    }
    
    shell_render_line();
    serial("[SHELL] History nav: pos=%d\n", ctx->hist_pos);
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
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    terminal_putchar('\n');
    
    if (ctx->line_len == 0) {
        shell_prompt();
        return;
    }
    
    /* Add to history */
    shell_history_add(ctx->line);
    ctx->hist_pos = -1; // Reset history navigation

    /* Pipe buffers */
    char* buf_in = 0;
    size_t len_in = 0;
    
    char* buf_out = 0;
    size_t len_out = 0;

    /* Split by pipe '|' */
    char* p = ctx->line;
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
    ctx->line[0] = 0;
    ctx->line_len = 0;
    ctx->cursor = 0;
    ctx->last_rendered_len = 0;
    
    shell_prompt();
}

static void shell_autocomplete() {
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    /* Find word start */
    int start = ctx->cursor;
    while (start > 0 && ctx->line[start-1] != ' ') start--;
    
    int word_len = ctx->cursor - start;
    if (word_len == 0) return;
    
    char prefix[64];
    int i;
    for(i=0; i<word_len && i<63; i++) prefix[i] = ctx->line[start+i];
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
            if (ctx->line_len < SHELL_BUF_SIZE - 1) {
                ctx->line[ctx->line_len++] = *rest;
                ctx->line[ctx->line_len] = 0;
                ctx->cursor++;
            }
            rest++;
        }
        /* Add space */
        if (ctx->line_len < SHELL_BUF_SIZE - 1) {
            ctx->line[ctx->line_len++] = ' ';
            ctx->line[ctx->line_len] = 0;
            ctx->cursor++;
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

static window_t* shell_win = NULL;

void shell_init() {
    /* Text Mode Init Only */
    shell_init_context(0);
    terminal_clear();
    serial("[SHELL] Initialized (Text Mode).\n");
    shell_prompt();
}

void shell_create_window() {
    int win_w = 640;
    int win_h = 400 + 24; /* +24 for title bar */
    surface_t* s = surface_create(win_w, win_h);
    if (s) {
        surface_clear(s, 0xFFFFFFFF); /* White background */
        
        /* Draw Title Bar  */
        fly_draw_rect_fill(s, 0, 0, win_w, 24, 0xFF000080); /* Classic Blue */
        const char* title = "Chrysalis OS : Konsole";
        int title_len = strlen(title);
        int title_x = (win_w - (title_len * 8)) / 2;
        fly_draw_text(s, title_x, 4, title, 0xFFFFFFFF);
        
        /* Close Button [X] */
        int bx = win_w - 20;
        int by = 4;
        fly_draw_rect_fill(s, bx, by, 16, 16, 0xFFC0C0C0); /* Classic Gray */
        fly_draw_rect_outline(s, bx, by, 16, 16, 0xFFFFFFFF);
        fly_draw_text(s, bx + 4, by, "X", 0xFFFFFFFF);

        shell_win = wm_create_window(s, 50, 50);
        
        terminal_set_surface(s);
        terminal_set_rect(0, 24, win_w, 400);
        
        serial("[SHELL] Window created and terminal attached.\n");
    } else {
        serial("[SHELL] Error: Failed to create surface (OOM?)\n");
    }
}

void shell_destroy_window() {
    if (shell_win) {
        wm_destroy_window(shell_win);
        shell_win = NULL;
        terminal_set_surface(NULL);
        terminal_set_rendering(false);
    }
}

int shell_is_window_active() {
    return (shell_win != NULL);
}

window_t* shell_get_window(void) {
    return shell_win;
}

bool shell_handle_event(input_event_t* ev) {
    if (!shell_win) return false;

    if (ev->type == INPUT_MOUSE_CLICK && ev->pressed) {
        int lx = ev->mouse_x - shell_win->x;
        int ly = ev->mouse_y - shell_win->y;
        
        /* Close Button Rect: x=W-20, y=4, w=16, h=16 */
        if (lx >= shell_win->w - 20 && lx <= shell_win->w - 4 && ly >= 4 && ly <= 20) {
            serial("[SHELL] Close button clicked.\n");
            shell_destroy_window();
            wm_mark_dirty();
            return true;
        }
    }
    return false;
}

void shell_init_context(int id) {
    if (id < 0 || id >= MAX_SHELL_CONTEXTS) return;
    shell_ctx_t* ctx = &contexts[id];
    ctx->line[0] = 0;
    ctx->line_len = 0;
    ctx->cursor = 0;
    ctx->hist_head = 0;
    ctx->hist_count = 0;
    ctx->hist_pos = -1;
    ctx->last_rendered_len = 0;
}

void shell_handle_char(char c) {
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    if (c == KEY_ENTER) {
        shell_exec_line();
        return;
    }
    
    if (c == KEY_BACKSPACE) {
        if (ctx->cursor > 0) {
            /* Shift left */
            for (int i = ctx->cursor; i < ctx->line_len; i++) {
                ctx->line[i-1] = ctx->line[i];
            }
            ctx->line_len--;
            ctx->cursor--;
            ctx->line[ctx->line_len] = 0;
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
        if (ctx->line_len < SHELL_BUF_SIZE - 1) {
            /* Insert at cursor */
            for (int i = ctx->line_len; i > ctx->cursor; i--) {
                ctx->line[i] = ctx->line[i-1];
            }
            ctx->line[ctx->cursor] = c;
            ctx->line_len++;
            ctx->cursor++;
            ctx->line[ctx->line_len] = 0;
            shell_render_line();
        }
    }
}

void shell_reset_input(void) {
    shell_ctx_t* ctx = &contexts[active_ctx_id];
    ctx->line[0] = 0;
    ctx->line_len = 0;
    ctx->cursor = 0;
}

void shell_set_active_context(int id) {
    if (id < 0 || id >= MAX_SHELL_CONTEXTS) return;
    active_ctx_id = id;
}

void shell_prompt(void) {
    user_t* u = user_get_current();
    char cwd[64];
    cd_get_cwd(cwd, 64);
    if (u) {
        terminal_printf("%s@chrysalis:%s$ ", u->name, cwd);
    } else {
        terminal_printf("guest@chrysalis:%s$ ", cwd);
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

/* kernel/cmds/cs.cpp
 * Chrysalis Script Interpreter (/bin/cs)
 *
 * Implements a simple line-by-line interpreter for .csr and .chs files.
 * Runs in "user mode" context (invoked via exec).
 */

#include "cs.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../fs/fs.h"
#include "../fs/chrysfs/chrysfs.h"
#include "registry.h"
#include "fat.h"

/* FAT32 Driver API */
extern "C" int fat32_read_file(const char* path, void* buf, uint32_t max_size);

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

/* Helper to execute a single command line */
static void cs_exec_line(char* line, int line_num) {
    /* Trim leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Ignore empty lines and comments */
    if (*line == 0 || *line == '#' || *line == '\n') return;

    /* Remove newline at end */
    int len = strlen(line);
    if (len > 0 && line[len-1] == '\n') line[len-1] = 0;

    serial("[CS] Executing line %d: %s\n", line_num, line);

    /* Tokenize */
    char* argv[32];
    int argc = 0;
    char* p = line;

    while (*p && argc < 32) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) break;

        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = 0;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = 0;
        }
    }

    if (argc > 0) {
        /* Dispatch command */
        /* We use the registry lookup helper */
        int found = 0;
        for (int i = 0; i < command_count; i++) {
            if (strcmp(command_table[i].name, argv[0]) == 0) {
                command_table[i].func(argc, argv);
                found = 1;
                break;
            }
        }
        if (!found) {
            terminal_printf("Script error line %d: Unknown command '%s'\n", line_num, argv[0]);
            serial("[CS] Error: Unknown command '%s'\n", argv[0]);
        }
    }
}

/* Helper to read file content (similar to exec.cpp but for text) */
static char* cs_read_script(const char* path, size_t* out_size) {
    /* 1. Try Disk (FAT32) */
    if (strncmp(path, "/root", 5) == 0) {
        fat_automount();

        size_t max_size = 64 * 1024; // 64KB script limit
        char* buf = (char*)kmalloc(max_size);
        if (!buf) return nullptr;

        int bytes = fat32_read_file(path, buf, max_size - 1);
        if (bytes > 0) {
            buf[bytes] = 0; // Null terminate text
            *out_size = (size_t)bytes;
            return buf;
        }
        kfree(buf);
    }

    /* 2. Try RAMFS */
    const FSNode* node = fs_find(path);
    if (node && node->data) {
        size_t len = strlen(node->data);
        char* buf = (char*)kmalloc(len + 1);
        if (!buf) return nullptr;
        strcpy(buf, node->data);
        *out_size = len;
        return buf;
    }

    return nullptr;
}

extern "C" int cmd_cs_main(int argc, char** argv) {
    serial("[CS] Interpreter started.\n");

    if (argc < 2) {
        terminal_writestring("Usage: cs <script.csr>\n");
        return -1;
    }

    const char* script_path = argv[1];
    serial("[CS] Opening script: %s\n", script_path);

    size_t script_size = 0;
    char* script_data = cs_read_script(script_path, &script_size);

    if (!script_data) {
        terminal_printf("cs: cannot open script '%s'\n", script_path);
        serial("[CS] Error: Cannot open script.\n");
        return -1;
    }

    /* Execute line by line */
    char* p = script_data;
    char* line_start = p;
    int line_num = 1;

    while (*p) {
        if (*p == '\n') {
            *p = 0; /* Terminate line */
            cs_exec_line(line_start, line_num++);
            line_start = p + 1;
        }
        p++;
    }
    
    /* Execute last line if no newline at EOF */
    if (p > line_start) {
        cs_exec_line(line_start, line_num);
    }

    kfree(script_data);
    serial("[CS] Script terminated.\n");
    return 0;
}
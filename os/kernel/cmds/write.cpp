/* kernel/cmds/write.cpp
 * Minimal bootstrap tool to create/append text files.
 */

#include "write.h"
#include "../terminal.h"
#include "../string.h"
#include "../mem/kmalloc.h"
#include "../fs/fs.h"
#include "../fs/chrysfs/chrysfs.h"
#include "fat.h"

/* FAT32 Driver API (External) */
extern "C" int fat32_create_file(const char* path, const void* data, uint32_t size);
extern "C" int fat32_read_file(const char* path, void* buf, uint32_t max_size);

/* Import serial logging */
extern "C" void serial(const char *fmt, ...);

/* Helper to join argv[start..argc] into a single string with spaces and a trailing newline */
static char* join_args(int argc, char** argv, int start_index) {
    int total_len = 0;
    for (int i = start_index; i < argc; i++) {
        total_len += strlen(argv[i]);
        if (i < argc - 1) total_len++; // space
    }
    total_len += 1; // newline
    
    char* str = (char*)kmalloc(total_len + 1);
    if (!str) return nullptr;
    
    char* p = str;
    for (int i = start_index; i < argc; i++) {
        const char* s = argv[i];
        while (*s) *p++ = *s++;
        if (i < argc - 1) *p++ = ' ';
    }
    *p++ = '\n';
    *p = 0;
    
    return str;
}

extern "C" int cmd_write(int argc, char** argv) {
    if (argc < 3) {
        terminal_writestring("Usage: write <path> <text...>\n");
        return -1;
    }

    const char* path = argv[1];
    char* new_text = join_args(argc, argv, 2);
    if (!new_text) {
        terminal_writestring("Error: Out of memory\n");
        return -1;
    }
    
    size_t new_len = strlen(new_text);

    /* Disk Handling (FAT32 via /root) */
    if (strncmp(path, "/root", 5) == 0) {
        /* Ensure partition is mounted */
        fat_automount();

        /* Try to read existing file to support append */
        size_t max_read = 64 * 1024; // 64KB limit for simple text files
        char* existing_buf = (char*)kmalloc(max_read);
        int existing_size = 0;
        
        if (existing_buf) {
            int r = fat32_read_file(path, existing_buf, max_read);
            if (r > 0) {
                existing_size = r;
            }
        }

        /* Allocate combined buffer */
        size_t total_size = existing_size + new_len;
        char* final_buf = (char*)kmalloc(total_size + 1);
        
        if (!final_buf) {
            terminal_writestring("Error: Out of memory for file write\n");
            kfree(new_text);
            if (existing_buf) kfree(existing_buf);
            return -1;
        }

        /* Combine: Old + New */
        if (existing_size > 0) {
            memcpy(final_buf, existing_buf, existing_size);
        }
        memcpy(final_buf + existing_size, new_text, new_len);
        final_buf[total_size] = 0;

        /* Write back to disk */
        int res = fat32_create_file(path, final_buf, total_size);
        
        if (res == 0) {
            if (existing_size > 0) {
                terminal_writestring("Appended to file.\n");
                serial("[WRITE] append %s: %d bytes\n", path, new_len);
            } else {
                terminal_writestring("Created file.\n");
                serial("[WRITE] created %s\n", path);
            }
        } else {
            terminal_writestring("Error writing file.\n");
            serial("[WRITE] error writing %s\n", path);
        }

        kfree(final_buf);
        if (existing_buf) kfree(existing_buf);
    } 
    /* RAMFS Handling (Fallback) */
    else {
        /* Check if exists to append */
        const FSNode* node = fs_find(path);
        char* final_buf = nullptr;
        
        if (node && node->data) {
            size_t old_len = strlen(node->data);
            final_buf = (char*)kmalloc(old_len + new_len + 1);
            if (final_buf) {
                strcpy(final_buf, node->data);
                strcat(final_buf, new_text);
            }
        } else {
            final_buf = (char*)kmalloc(new_len + 1);
            if (final_buf) {
                strcpy(final_buf, new_text);
            }
        }

        if (final_buf) {
            /* fs_create typically creates or updates in RAMFS */
            fs_create(path, final_buf);
            
            if (node) {
                terminal_writestring("Appended to RAMFS file.\n");
                serial("[WRITE] append ramfs %s\n", path);
            } else {
                terminal_writestring("Created RAMFS file.\n");
                serial("[WRITE] created ramfs %s\n", path);
            }
            
            /* Note: We don't free final_buf here assuming RAMFS takes ownership 
               or copies. If it copies, this is a small leak, acceptable for bootstrap. */
        } else {
             terminal_writestring("Error: Out of memory\n");
        }
    }

    kfree(new_text);
    return 0;
}
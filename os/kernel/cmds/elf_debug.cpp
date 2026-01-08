// kernel/cmds/elf_debug.cpp
#include "elf_debug.h"
#include <stdint.h>
#include <stddef.h>
#include "../terminal.h"
#include "../fs/fs.h"
#include "../fs/chrysfs/chrysfs.h"
#include "../mem/kmalloc.h"
#include "../string.h"

static const char* flags_to_str(uint32_t flags) {
    static char buf[8];
    int p = 0;
    buf[0] = '\0';
    if (flags & PF_R) { buf[p++] = 'r'; }
    if (flags & PF_W) { buf[p++] = 'w'; }
    if (flags & PF_X) { buf[p++] = 'x'; }
    buf[p] = '\0';
    return buf;
}

void elf_debug_print_info(const elf_load_info_t* info) {
    if (!info) {
        terminal_printf("[elf_debug] null info\n");
        return;
    }
    const elf32_ehdr_t* h = &info->ehdr;
    terminal_printf("ELF: entry=0x%x phnum=%u phentsize=%u\n", info->entry_point, (unsigned)h->e_phnum, (unsigned)h->e_phentsize);
    terminal_printf("ELF: e_type=%u machine=%u version=%u\n", (unsigned)h->e_type, (unsigned)h->e_machine, (unsigned)h->e_version);
    terminal_printf("Segments: count=%d\n", info->seg_count);
    for (int i = 0; i < info->seg_count; ++i) {
        const elf_segment_t* s = &info->segments[i];
        terminal_printf("  seg[%d]: vaddr=0x%x filesz=%u memsz=%u flags=%s kernel_buf=%s\n",
                        i, (unsigned)s->vaddr, (unsigned)s->filesz, (unsigned)s->memsz,
                        flags_to_str(s->flags),
                        (s->kernel_buf ? "yes" : "no"));
    }
}

/* Helper to read file content into a buffer */
static uint8_t* read_file_debug(const char* path, uint32_t* out_size) {
    /* 1. Try ChrysFS (Disk) */
    if (strncmp(path, "/root", 5) == 0) {
        size_t max_size = 1024 * 1024; // 1MB limit
        uint8_t* buf = (uint8_t*)kmalloc(max_size);
        if (!buf) return NULL;

        int bytes = chrysfs_read_file(path, (char*)buf, max_size);
        if (bytes > 0) {
            *out_size = (uint32_t)bytes;
            return buf;
        }
        kfree(buf);
    }

    /* 2. Try RAMFS */
    const FSNode* node = fs_find(path);
    if (node && node->data) {
        size_t len = strlen(node->data);
        uint8_t* buf = (uint8_t*)kmalloc(len);
        if (!buf) return NULL;
        memcpy(buf, node->data, len);
        *out_size = (uint32_t)len;
        return buf;
    }

    return NULL;
}

/* optional small command wrapper so you can call `elf_debug <file>` directly */
extern "C" int cmd_elf_debug(int argc, char** argv) {
    if (argc < 2) {
        terminal_printf("usage: elf_debug <file>\n");
        return -1;
    }
    const char* path = argv[1];
    uint32_t len = 0;
    
    uint8_t* buf = read_file_debug(path, &len);
    if (!buf) {
        terminal_printf("[elf_debug] cannot read %s\n", path);
        return -1;
    }
    elf_load_info_t info;
    int r = elf_load_from_buffer(buf, len, &info);
    if (r != 0) {
        terminal_printf("[elf_debug] parse failed: %d\n", r);
        kfree(buf);
        return -1;
    }
    elf_debug_print_info(&info);
    elf_unload_kernel_space(&info);
    kfree(buf);
    return 0;
}

/* kernel/proc/exec.cpp
 * ELF Loader and Execution for ChrysalisOS
 */

#include "exec.h"
#include "../fs/fs.h"
#include "../fs/chrysfs/chrysfs.h"
#include "../mem/kmalloc.h"
#include "../string.h"
#include "../terminal.h"
#include "../mm/vmm.h"
#include "../mm/paging.h"
#include "../memory/pmm.h"

/* ELF Header Definitions */
#define ELF_MAGIC 0x464C457F

typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;

typedef struct {
    uint8_t     e_ident[16];
    Elf32_Half  e_type;
    Elf32_Half  e_machine;
    Elf32_Word  e_version;
    Elf32_Addr  e_entry;
    Elf32_Off   e_phoff;
    Elf32_Off   e_shoff;
    Elf32_Word  e_flags;
    Elf32_Half  e_ehsize;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} Elf32_Phdr;

#define PT_LOAD 1

/* Helper to read file content into a buffer */
static uint8_t* read_executable(const char* path, size_t* out_size) {
    /* 1. Try ChrysFS (Disk) */
    if (strncmp(path, "/root", 5) == 0) {
        /* Allocate a reasonable buffer for the binary */
        size_t max_size = 1024 * 1024; // 1MB limit for now
        uint8_t* buf = (uint8_t*)kmalloc(max_size);
        if (!buf) return nullptr;

        int bytes = chrysfs_read_file(path, (char*)buf, max_size);
        if (bytes > 0) {
            *out_size = (size_t)bytes;
            return buf;
        }
        kfree(buf);
    }

    /* 2. Try RAMFS */
    const FSNode* node = fs_find(path);
    if (node && node->data) {
        size_t len = strlen(node->data); // RAMFS stores text/data as string currently
        /* Copy to new buffer to be safe/uniform */
        uint8_t* buf = (uint8_t*)kmalloc(len);
        if (!buf) return nullptr;
        memcpy(buf, node->data, len);
        *out_size = len;
        return buf;
    }

    return nullptr;
}

extern "C" int execve(const char *filename, char *const argv[], char *const envp[]) {
    (void)argv;
    (void)envp;

    terminal_printf("[EXEC] Loading '%s'...\n", filename);

    size_t file_size = 0;
    uint8_t* file_data = read_executable(filename, &file_size);

    if (!file_data) {
        terminal_printf("[EXEC] Error: Could not read file '%s'\n", filename);
        return -1;
    }

    /* Verify ELF Header */
    if (file_size < sizeof(Elf32_Ehdr)) {
        terminal_printf("[EXEC] Error: File too small\n");
        kfree(file_data);
        return -1;
    }

    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)file_data;
    if (*(uint32_t*)ehdr->e_ident != ELF_MAGIC) {
        terminal_printf("[EXEC] Error: Not a valid ELF binary\n");
        kfree(file_data);
        return -1;
    }

    if (ehdr->e_type != 2) { /* ET_EXEC */
        terminal_printf("[EXEC] Warning: ELF type is not ET_EXEC (type=%d). Trying anyway...\n", ehdr->e_type);
    }

    /* Load Segments */
    Elf32_Phdr* phdr = (Elf32_Phdr*)(file_data + ehdr->e_phoff);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            terminal_printf("[EXEC] Segment %d: FileOff=0x%x VAddr=0x%x FileSz=0x%x MemSz=0x%x\n",
                            i, phdr[i].p_offset, phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);

            /* Map memory for this segment */
            
            uint32_t start_page = phdr[i].p_vaddr & PAGE_FRAME_MASK;
            uint32_t end_page = (phdr[i].p_vaddr + phdr[i].p_memsz + PAGE_SIZE - 1) & PAGE_FRAME_MASK;
            
            for (uint32_t page = start_page; page < end_page; page += PAGE_SIZE) {
                /* Check if mapped, if not allocate */
                if (page >= 0xC0000000) {
                    terminal_printf("[EXEC] Error: Segment overlaps kernel memory (0x%x)\n", page);
                    kfree(file_data);
                    return -1;
                }

                /* Allocate a frame */
                void* new_page = vmm_alloc_page();
                if (!new_page) {
                    terminal_printf("[EXEC] Error: OOM during load\n");
                    kfree(file_data);
                    return -1;
                }
                
                /* Map it at the requested virtual address */
                uint32_t phys = vmm_virt_to_phys(new_page);
                vmm_map_page(kernel_page_directory, page, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            }

            /* Copy data */
            memset((void*)phdr[i].p_vaddr, 0, phdr[i].p_memsz);
            memcpy((void*)phdr[i].p_vaddr, file_data + phdr[i].p_offset, phdr[i].p_filesz);
        }
    }

    /* Entry Point */
    void (*entry_point)(void) = (void (*)(void))ehdr->e_entry;
    terminal_printf("[EXEC] Jumping to entry point: 0x%x\n", entry_point);

    /* Cleanup buffer */
    kfree(file_data);

    /* Execute */
    entry_point();

    /* If the program returns */
    terminal_printf("[EXEC] Program exited.\n");
    return 0;
}

/* Compatibility wrapper for existing elf.cpp command */
extern "C" int exec_from_path(const char* path, char* const argv[]) {
    return execve(path, argv, nullptr);
}

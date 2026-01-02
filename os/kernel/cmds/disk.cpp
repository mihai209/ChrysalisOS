/* fixed disk.cpp - robust argument parsing for freestanding OS */
#include "disk.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "../storage/ata.h"
#include "../terminal.h"
#include "../fs/fat/fat.h"

/* Helpers */
static size_t k_strlen(const char* s) {
    const char* p = s; while (*p) ++p; return (size_t)(p - s);
}
static int k_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void* k_memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

static char* u32_to_dec(char* out, uint32_t v) {
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; return out + 1; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = ti - 1, j = 0; i >= 0; --i, ++j) out[j] = tmp[i];
    return out + ti;
}

static char* i32_to_dec(char* out, int32_t v) {
    if (v < 0) { *out++ = '-'; return u32_to_dec(out, (uint32_t)(-v)); }
    return u32_to_dec(out, (uint32_t)v);
}

static int k_sprintf(char* out, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char* p = out;
    const char* f = fmt;
    while (*f) {
        if (*f != '%') { *p++ = *f++; continue; }
        f++;
        int zero_pad = 0, width = 0;
        if (*f == '0') { zero_pad = 1; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }
        char spec = *f++;
        if (spec == '\0') break;
        if (spec == 'u' || spec == 'd') {
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[16]; char *q = (spec == 'u') ? u32_to_dec(tmp, v) : i32_to_dec(tmp, (int)v);
            int len = (int)(q - tmp);
            int pad = width - len;
            while (pad-- > 0) *p++ = zero_pad ? '0' : ' ';
            k_memcpy(p, tmp, (size_t)len); p += len;
        } else if (spec == 'x' || spec == 'X') {
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[16]; int ti = 0;
            if (v == 0) tmp[ti++] = '0';
            else {
                while (v) {
                    int d = v & 0xF;
                    tmp[ti++] = (char)(d < 10 ? ('0' + d) : ((spec == 'x' ? 'a' : 'A') + (d - 10)));
                    v >>= 4;
                }
            }
            int pad = width - ti;
            while (pad-- > 0) *p++ = zero_pad ? '0' : ' ';
            for (int i = ti - 1; i >= 0; --i) *p++ = tmp[i];
        } else if (spec == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s) *p++ = *s++;
        } else if (spec == 'c') {
            *p++ = (char)va_arg(ap, int);
        } else {
            *p++ = '%'; *p++ = spec;
        }
    }
    *p = '\0'; va_end(ap);
    return (int)(p - out);
}

#define memcpy k_memcpy
#define sprintf k_sprintf
#define strlen k_strlen
#define strcmp k_strcmp

static void println(const char* s) { terminal_writestring((char*)s); terminal_writestring("\n"); }
static void print(const char* s)  { terminal_writestring((char*)s); }

static uint32_t parse_u32(const char* s) {
    if (!s) return 0;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; }
    return v;
}

static void hexdump(const uint8_t* buf, int count) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < count; i += 16) {
        for (int j = 0; j < 16 && (i + j) < count; ++j) {
            uint8_t b = buf[i + j];
            char out[4] = { hex[b >> 4], hex[b & 0xF], ' ', '\0' };
            print(out);
        }
        print("\n");
    }
}

/* MODIFIED: split_args now handles \n and \r */
static int split_args(const char* args_orig, char* argv[], int max_args, char* store, int store_size) {
    if (!args_orig || args_orig[0] == '\0') return 0;
    int n = strlen(args_orig);
    if (n + 1 > store_size) n = store_size - 1;
    memcpy(store, args_orig, n);
    store[n] = '\0';

    int argc = 0;
    char* p = store;
    while (argc < max_args && p && *p) {
        // Sari peste orice whitespace (spațiu, tab, newline, carriage return)
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (!*p) break;

        argv[argc++] = p;
        char* q = p;
        // Mergi până la următorul whitespace
        while (*q && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') q++;
        
        if (*q) { 
            *q = '\0'; 
            p = q + 1; 
        } else { 
            p = NULL; 
        }
    }
    return argc;
}

struct part_assign g_assigns[26];

static void cmd_usage() {
    println("disk --list              : show device info");
    println("disk --usage / --help    : this help");
    println("disk --init              : write MBR (start LBA 2048)");
    println("disk --scan              : scan MBR partitions");
    println("disk --assign <let> <lba> <cnt> : manual assign");
    println("disk --format <let>      : quick-zero sectors");
    println("disk --test --write <lba>: test write");
}

static void cmd_list(int debug) {
    uint16_t idbuf[256];
    if (ata_identify(idbuf) != 0) { println("[disk] identify failed"); return; }
    char model[48]; uint32_t sectors = 0;
    ata_decode_identify(idbuf, model, sizeof(model), &sectors);
    char tmp[128];
    sprintf(tmp, "[disk] model: %s\n[disk] LBA28 sectors: %u\n", model, sectors);
    print(tmp);
    if (debug) {
        sprintf(tmp, "[disk-debug] id[60-61]: 0x%04x%04x\n", idbuf[61], idbuf[60]);
        print(tmp);
    }
    println("[disk] assignments:");
    for (int i = 0; i < 26; ++i) {
        if (g_assigns[i].used) {
            sprintf(tmp, "  %c => LBA %u count %u\n", g_assigns[i].letter, g_assigns[i].lba, g_assigns[i].count);
            print(tmp);
        }
    }
}

static int create_minimal_mbr() {
    uint16_t idbuf[256];
    if (ata_identify(idbuf) != 0) return -1;
    uint32_t sectors = ((uint32_t)idbuf[61] << 16) | (uint32_t)idbuf[60];
    if (sectors == 0) return -2;

    uint32_t start = 2048;
    uint32_t part_size = (sectors > start) ? (sectors - start) : 0;

    uint8_t mbr[512];
    for (int i = 0; i < 512; ++i) mbr[i] = 0;
    int off = 446;
    mbr[off + 4] = 0x0C; // FAT32 LBA
    mbr[off + 8] = (uint8_t)(start & 0xFF); mbr[off + 9] = (uint8_t)((start >> 8) & 0xFF);
    mbr[off + 10] = (uint8_t)((start >> 16) & 0xFF); mbr[off + 11] = (uint8_t)((start >> 24) & 0xFF);
    mbr[off + 12] = (uint8_t)(part_size & 0xFF); mbr[off + 13] = (uint8_t)((part_size >> 8) & 0xFF);
    mbr[off + 14] = (uint8_t)((part_size >> 16) & 0xFF); mbr[off + 15] = (uint8_t)((part_size >> 24) & 0xFF);
    mbr[510] = 0x55; mbr[511] = 0xAA;

    ata_set_allow_mbr_write(1);
    int w = ata_write_sector(0, mbr);
    ata_set_allow_mbr_write(0);
    if (w == 0) println("[disk] MBR written");
    return w;
}

static void cmd_scan() {
    uint8_t mbr[512];
    if (ata_read_sector(0, mbr) != 0 || mbr[510] != 0x55 || mbr[511] != 0xAA) {
        println("[disk] no valid MBR"); return;
    }
    for (int i = 0; i < 4; ++i) {
        int off = 446 + i * 16;
        uint8_t type = mbr[off + 4];
        if (type == 0) continue;
        uint32_t lba = *(uint32_t*)&mbr[off + 8];
        uint32_t cnt = *(uint32_t*)&mbr[off + 12];
        g_assigns[i].used = 1; g_assigns[i].letter = 'a' + i;
        g_assigns[i].lba = lba; g_assigns[i].count = cnt;
        char tmp[128];
        sprintf(tmp, "[disk] Found partition %d: start %u, assigned '%c'\n", i, lba, 'a' + i);
        print(tmp);
    }
}

/* ENTRYPOINT */
void cmd_disk(const char* args) {
    char store[256];
    char* argv[16];
    int argc = split_args(args, argv, 16, store, sizeof(store));
    if (argc == 0) { cmd_usage(); return; }

    /* REPAIR: Determine index of the first flag */
    int idx = 0;
    if (strcmp(argv[0], "disk") == 0) {
        idx = 1;
        if (argc == 1) { cmd_usage(); return; }
    }

    const char* cmd = argv[idx];

    if (strcmp(cmd, "--list") == 0) {
        int dbg = (idx + 1 < argc && strcmp(argv[idx+1], "--debug") == 0);
        cmd_list(dbg);
    } 
    else if (strcmp(cmd, "--init") == 0) {
        println("[disk] initializing...");
        create_minimal_mbr();
        cmd_scan();
    }
    else if (strcmp(cmd, "--scan") == 0) {
        cmd_scan();
    }
    else if (strcmp(cmd, "--assign") == 0) {
        if (argc < idx + 4) { println("disk --assign <let> <lba> <cnt>"); return; }
        int letter_idx = argv[idx+1][0] - 'a';
        if (letter_idx >= 0 && letter_idx < 26) {
            g_assigns[letter_idx].used = 1;
            g_assigns[letter_idx].letter = argv[idx+1][0];
            g_assigns[letter_idx].lba = parse_u32(argv[idx+2]);
            g_assigns[letter_idx].count = parse_u32(argv[idx+3]);
            println("[disk] assigned");
        }
    }
    else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "--usage") == 0) {
        cmd_usage();
    }
    else {
        print("[disk] Unknown: "); println(cmd);
        cmd_usage();
    }
}
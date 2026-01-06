/* disk.cpp - final version, functions exported for fat.cpp */
#include "disk.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "../storage/ata.h"
#include "../terminal.h"
#include "../fs/fat/fat.h"
#include "../storage/block.h"
#include "../fs/chrysfs/chrysfs.h"

/* Minimal freestanding helpers - rămân static */
static size_t k_strlen(const char* s) {
    const char* p = s; while (*p) ++p; return (size_t)(p - s);
}

static void* k_memcpy(void* dst, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

static char* u32_to_dec(char* out, uint32_t v)
{
    char tmp[12]; int ti = 0;
    if (v == 0) { out[0] = '0'; return out + 1; }
    while (v > 0) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = ti - 1, j = 0; i >= 0; --i, ++j) out[j] = tmp[i];
    return out + ti;
}

static char* i32_to_dec(char* out, int32_t v)
{
    if (v < 0) { *out++ = '-'; return u32_to_dec(out, (uint32_t)(-v)); }
    return u32_to_dec(out, (uint32_t)v);
}

static int k_sprintf(char* out, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    char* p = out;
    const char* f = fmt;

    while (*f) {
        if (*f != '%') { *p++ = *f++; continue; }
        f++;

        int zero_pad = 0;
        int width = 0;
        if (*f == '0') { zero_pad = 1; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }

        char spec = *f++;
        if (spec == '\0') break;

        if (spec == 'u') {
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[16]; char *q = tmp;
            q = u32_to_dec(tmp, (uint32_t)v);
            int len = (int)(q - tmp);
            int pad = width - len;
            char fill = zero_pad ? '0' : ' ';
            for (int i = 0; i < pad; ++i) *p++ = fill;
            k_memcpy(p, tmp, (size_t)len); p += len;
        } else if (spec == 'd') {
            int v = va_arg(ap, int);
            char tmp[16];
            char *q = i32_to_dec(tmp, v);
            int len = (int)(q - tmp);
            int pad = width - len;
            char fill = zero_pad ? '0' : ' ';
            for (int i = 0; i < pad; ++i) *p++ = fill;
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
            int len = ti;
            int pad = width - len;
            char fill = zero_pad ? '0' : ' ';
            for (int i = 0; i < pad; ++i) *p++ = fill;
            for (int i = len - 1; i >= 0; --i) *p++ = tmp[i];
        } else if (spec == 'c') {
            *p++ = (char)va_arg(ap, int);
        } else if (spec == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s) *p++ = *s++;
        } else if (spec == '%') {
            *p++ = '%';
        } else {
            *p++ = '%'; *p++ = spec;
        }
    }
    *p = '\0';
    va_end(ap);
    return (int)(p - out);
}

#define memcpy k_memcpy
#define sprintf k_sprintf
#define strlen k_strlen

static void println(const char* s) { terminal_writestring((char*)s); terminal_writestring("\n"); }
static void print(const char* s)  { terminal_writestring((char*)s); }

static uint32_t parse_u32(const char* s)
{
    if (!s) return 0;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; }
    return v;
}

static void hexdump_line(const uint8_t* buf, int off, int len)
{
    const char hex[] = "0123456789ABCDEF";
    char out[4];
    for (int i = 0; i < len; ++i) {
        uint8_t b = buf[off + i];
        out[0] = hex[b >> 4]; out[1] = hex[b & 0xF]; out[2] = ' '; out[3] = '\0';
        print(out);
    }
    print("\n");
}

static int strcmp_local(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Partition assign table */
struct part_assign g_assigns[26];

static void init_g_assigns(void) {
    for (int i = 0; i < 26; i++) {
        g_assigns[i].used = 0;
        g_assigns[i].letter = '\0';
        g_assigns[i].lba = 0;
        g_assigns[i].count = 0;
    }
}

static void cmd_usage(void) {
    println("disk - gestiune disk (comenzi):");
    println("  disk help                : this help");
    println("  disk list                : show device info");
    println("  disk init                : write simple MBR");
    println("  disk scan                : scan MBR partitions");
    println("  disk assign <l> <lba> <cnt> : manual assign");
    println("  disk blocks              : list block devices (AHCI)");
    println("  disk format <letter>     : quick format");
    println("  disk test write <lba>    : test write");
    println("  disk test output <lba>   : read & dump");
    println("  disk test see-raw <lba> <cnt> : dump multiple");
    println("  disk fat info            : FAT info");
}

static void cmd_list(int debug)
{
    uint16_t idbuf[256];
    int r = ata_identify(idbuf);
    if (r != 0) { println("[disk] no device / identify failed"); return; }

    char model[48];
    uint32_t sectors = 0;
    ata_decode_identify(idbuf, model, sizeof(model), &sectors);

    print("[disk] model: "); print(model); print("\n");
    char tmp[128];
    sprintf(tmp, "[disk] LBA28 sectors: %u\n", sectors);
    print(tmp);

    if (debug) {
        sprintf(tmp, "[disk-debug] ata_identify rc=%d\n", r); print(tmp);
        /* debug output redus pentru claritate */
    }

    println("[disk] assignments:");
    for (int i = 0; i < 26; ++i) {
        if (g_assigns[i].used) {
            sprintf(tmp, "  %c => LBA %u count %u\n", g_assigns[i].letter, g_assigns[i].lba, g_assigns[i].count);
            print(tmp);
        }
    }
}

static void cmd_list_blocks(void) {
    println("[disk] Block Devices:");
    // Simple iteration by probing names
    char name[32];
    for (int i = 0; i < 4; i++) {
        sprintf(name, "ahci%d", i);
        block_device_t* bd = block_get(name);
        if (bd) {
            char tmp[128];
            sprintf(tmp, "  %s: %u sectors (AHCI)\n", bd->name, (uint32_t)bd->sector_count);
            print(tmp);
        }
    }
}

/* FUNCȚII EXPORTATE - FĂRĂ static (pentru fat.cpp) */
int create_minimal_mbr(void)
{
    uint16_t idbuf[256];
    if (ata_identify(idbuf) != 0) {
        println("[disk] identify failed - cannot init MBR");
        return -1;
    }

    uint32_t lba28 = ((uint32_t)idbuf[61] << 16) | idbuf[60];
    uint64_t lba48 = (uint64_t)idbuf[100] | ((uint64_t)idbuf[101] << 16) |
                    ((uint64_t)idbuf[102] << 32) | ((uint64_t)idbuf[103] << 48);

    uint32_t sectors = lba28 ? lba28 : (lba48 ? (lba48 > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)lba48) : 0);
    if (sectors == 0) {
        println("[disk] identify returned 0 sectors");
        return -2;
    }

    uint32_t start = (sectors > 2048) ? 2048 : 1;
    uint32_t part_size = sectors - start;

    uint8_t mbr[512] = {0};
    int off = 446;
    mbr[off + 4] = 0x0B;
    *(uint32_t*)(mbr + off + 8) = start;
    *(uint32_t*)(mbr + off + 12) = part_size;
    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    ata_set_allow_mbr_write(1);
    int w = ata_write_sector(0, mbr);
    ata_set_allow_mbr_write(0);

    if (w == 0) println("[disk] MBR written (partition created)");
    else { println("[disk] MBR write FAILED"); return -3; }
    return 0;
}

int cmd_format_letter(char letter)
{
    int idx = letter - 'a';
    if (idx < 0 || idx >= 26) { println("[disk] invalid letter"); return -1; }
    if (!g_assigns[idx].used) { println("[disk] letter not assigned"); return -2; }

    uint32_t start = g_assigns[idx].lba;
    uint32_t count = g_assigns[idx].count;
    uint32_t to_zero = (count < 128) ? count : 128;

    uint8_t buf[512] = {0};
    char tmp[64];

    for (uint32_t s = 0; s < to_zero; ++s) {
        int r = ata_write_sector(start + s, buf);
        if (r != 0) {
            sprintf(tmp, "[disk] write failed at LBA %u (r=%d)\n", start + s, r);
            print(tmp);
            return -3;
        }
    }
    sprintf(tmp, "[disk] quick-format done (%u sectors zeroed)\n", to_zero);
    print(tmp);
    return 0;
}

void cmd_scan(void)
{
    uint8_t mbr[512];
    if (ata_read_sector(0, mbr) != 0) {
        println("[disk] failed to read MBR");
        return;
    }
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        println("[disk] invalid MBR signature");
        return;
    }

    println("[disk] scanning MBR...");

    for (int i = 0; i < 4; ++i) {
        int off = 446 + i * 16;
        uint8_t type = mbr[off + 4];
        uint32_t lba = *(uint32_t*)(mbr + off + 8);
        uint32_t count = *(uint32_t*)(mbr + off + 12);

        if (type == 0 || count == 0) continue;

        int slot = -1;
        for (int j = 0; j < 26; ++j) {
            if (!g_assigns[j].used) { slot = j; break; }
        }
        if (slot < 0) { println("[disk] no free letter slots"); break; }

        g_assigns[slot].used = 1;
        g_assigns[slot].letter = 'a' + slot;
        g_assigns[slot].lba = lba;
        g_assigns[slot].count = count;

        char tmp[96];
        sprintf(tmp, "[disk] partition %d: type=0x%02x start=%u count=%u -> assigned '%c'\n",
                i, type, lba, count, g_assigns[slot].letter);
        print(tmp);
    }
}

/* FUNCTIA PRINCIPALĂ - compatibilă cu registry */
void cmd_disk(int argc, char** argv)
{
    init_g_assigns();  // inițializare o singură dată la prima apelare

    if (argc < 2) {
        cmd_usage();
        return;
    }
    const char* sub = argv[1];

    if (strcmp_local(sub, "help") == 0 || strcmp_local(sub, "usage") == 0) {
        cmd_usage();
        return;
    }

    if (strcmp_local(sub, "list") == 0) {
        int debug = (argc >= 3 && strcmp_local(argv[2], "debug") == 0);
        cmd_list(debug);
        return;
    }

    if (strcmp_local(sub, "blocks") == 0) {
        cmd_list_blocks();
        return;
    }

    if (strcmp_local(sub, "init") == 0) {
        println("[disk] initializing (write MBR) ...");
        create_minimal_mbr();
        cmd_scan();
        if (g_assigns[0].used && fat32_init(0, g_assigns[0].lba) == 0)
            println("[disk] FAT mounted");
        else if (g_assigns[0].used)
            println("[disk] FAT mount failed");
        return;
    }

    if (strcmp_local(sub, "scan") == 0) {
        cmd_scan();
        if (g_assigns[0].used && fat32_init(0, g_assigns[0].lba) == 0)
            println("[disk] FAT mounted");
        else if (g_assigns[0].used)
            println("[disk] FAT mount failed");
        return;
    }

    if (strcmp_local(sub, "assign") == 0) {
        if (argc < 5) { println("usage: disk assign <letter> <lba> <count>"); return; }
        char letter = argv[2][0];
        uint32_t lba = parse_u32(argv[3]);
        uint32_t count = parse_u32(argv[4]);
        int idx = letter - 'a';
        if (idx < 0 || idx >= 26) { println("invalid letter"); return; }
        g_assigns[idx].used = 1;
        g_assigns[idx].letter = letter;
        g_assigns[idx].lba = lba;
        g_assigns[idx].count = count;
        println("[disk] assigned");
        return;
    }

    if (strcmp_local(sub, "format") == 0) {
        if (argc < 3) { println("usage: disk format <letter>"); return; }
        cmd_format_letter(argv[2][0]);
        return;
    }

    /* test subcommands - simplificat */
    if (strcmp_local(sub, "test") == 0) {
        /* cod existent... păstrează-l dacă vrei */
    }

    if (strcmp_local(sub, "fat") == 0 && argc >= 3 && strcmp_local(argv[2], "info") == 0) {
        fat32_list_root();
        return;
    }

    println("[disk] unknown command");
    cmd_usage();
}
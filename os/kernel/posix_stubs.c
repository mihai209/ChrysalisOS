#include "include/stdio.h"
#include "include/sys/stat.h"
#include <stddef.h>
#include "include/fcntl.h"
#include "include/unistd.h"
#include "include/sys/time.h"
#include <stdarg.h>
#include "terminal.h" /* Pentru terminal_putchar, terminal_vprintf */
#include "include/stdlib.h"
#include "include/math.h"
#include "include/errno.h"
#include "mem/kmalloc.h"
#include "cmds/fat.h"
#include "string.h"
#include "include/setjmp.h"

extern void serial(const char *fmt, ...);

/* Declarăm funcția din doom_iwad.c */
extern int doom_iwad_open(const char* name, void** data, size_t* size);

/* Stub-uri POSIX minimale pentru compilarea DoomGeneric */

/* FILE este definit în stdio.h */

int mkdir(const char* path, mode_t mode) {
    (void)mode;
    fat_automount();
    return fat32_create_directory(path);
}

typedef struct {
    char path[128];
    uint32_t pos;
    uint32_t size;
    int mode; /* 0=read, 1=write */
    void* write_buf;
    uint32_t write_cap;
    const void* mem_ptr; /* Pointer pentru fișiere embedded (read-only) */
} internal_file_t;

FILE* fopen(const char* filename, const char* mode) {
    /* 1. Verificăm dacă este IWAD-ul embedded */
    void* iwad_data = NULL;
    size_t iwad_size = 0;
    if (strchr(mode, 'r') && doom_iwad_open(filename, &iwad_data, &iwad_size)) {
        serial("[POSIX] fopen: Serving embedded %s (%u bytes)\n", filename, iwad_size);
        if (iwad_size < 4 * 1024 * 1024) {
            serial("[POSIX] WARNING: IWAD size is suspiciously small (%u bytes). Doom may crash.\n", iwad_size);
        }

        internal_file_t* f = (internal_file_t*)kmalloc(sizeof(internal_file_t));
        if (!f) return NULL;
        memset(f, 0, sizeof(internal_file_t));
        strncpy(f->path, filename, 127);
        f->size = (uint32_t)iwad_size;
        f->mode = 0;
        f->mem_ptr = iwad_data; /* Setăm pointerul direct */
        return (FILE*)f;
    }

    /* 2. Altfel, încercăm FAT32 */
    fat_automount();
    
    if (strchr(mode, 'w')) {
        /* Write Mode: Buffer in memory, flush on close */
        internal_file_t* f = (internal_file_t*)kmalloc(sizeof(internal_file_t));
        if (!f) return NULL;
        memset(f, 0, sizeof(internal_file_t));
        strncpy(f->path, filename, 127);
        f->mode = 1;
        f->write_cap = 512 * 1024; /* 512KB limit for savegames */
        f->write_buf = kmalloc(f->write_cap);
        if (!f->write_buf) { kfree(f); return NULL; }
        return (FILE*)f;
    } else {
        /* Read Mode */
        int32_t sz = fat32_get_file_size(filename);
        if (sz < 0) {
            /* terminal_printf("[POSIX] fopen failed: %s\n", filename); */
            return NULL;
        }

        internal_file_t* f = (internal_file_t*)kmalloc(sizeof(internal_file_t));
        if (!f) return NULL;
        memset(f, 0, sizeof(internal_file_t));
        strncpy(f->path, filename, 127);
        f->size = (uint32_t)sz;
        f->mode = 0;
        return (FILE*)f;
    }
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    internal_file_t* f = (internal_file_t*)stream;
    if (!f || f->mode != 0) return 0;

    size_t bytes_req = size * nmemb;
    if (bytes_req == 0) return 0;
    
    /* Cazul fișier embedded în memorie */
    if (f->mem_ptr) {
        size_t bytes_available = 0;
        if (f->pos < f->size) bytes_available = f->size - f->pos;
        
        size_t bytes_to_read = bytes_req;
        if (bytes_to_read > bytes_available) bytes_to_read = bytes_available;
        
        memcpy(ptr, (const uint8_t*)f->mem_ptr + f->pos, bytes_to_read);
        
        /* Debug: WAD Header check */
        if (f->pos == 0 && bytes_to_read >= 12) {
            uint8_t* b = (uint8_t*)ptr;
            uint32_t numlumps = *(uint32_t*)(b + 4);
            uint32_t infotableofs = *(uint32_t*)(b + 8);
            serial("[FS] WAD Header: %.4s, numlumps=%u, infotableofs=%u\n", b, numlumps, infotableofs);
        }

        f->pos += bytes_to_read;
        return bytes_to_read / size;
    }
    
    int bytes_read = fat32_read_file_offset(f->path, ptr, bytes_req, f->pos);
    
    if (bytes_read < 0) return 0;
    
    /* Log short reads on FAT32 */
    if ((size_t)bytes_read != bytes_req) {
        serial("[FS] Warning: Short read on %s: req=%u read=%d pos=%u\n", f->path, bytes_req, bytes_read, f->pos);
    }

    f->pos += bytes_read;
    return bytes_read / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    internal_file_t* f = (internal_file_t*)stream;
    if (!f || f->mode != 1) return 0;
    
    uint32_t bytes = size * nmemb;
    if (f->pos + bytes > f->write_cap) return 0; /* Overflow */
    
    memcpy((uint8_t*)f->write_buf + f->pos, ptr, bytes);
    f->pos += bytes;
    if (f->pos > f->size) f->size = f->pos;
    return nmemb;
}

int fseek(FILE* stream, long offset, int whence) {
    internal_file_t* f = (internal_file_t*)stream;
    if (!f) return -1;
    
    long new_pos = (long)f->pos;
    
    switch (whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos += offset; break;
        case SEEK_END: new_pos = (long)f->size + offset; break;
        default: return -1;
    }
    
    /* Allow seeking past EOF (standard behavior), but not before start */
    if (new_pos < 0) return -1;
    
    f->pos = (uint32_t)new_pos;
    /* serial("[FS] fseek: whence=%d off=%ld -> new_pos=%u\n", whence, offset, f->pos); */
    return 0;
}

long ftell(FILE* stream) {
    internal_file_t* f = (internal_file_t*)stream;
    if (!f) return -1;
    return f->pos;
}

int fclose(FILE* stream) {
    internal_file_t* f = (internal_file_t*)stream;
    if (!f) return -1;
    
    if (f->mode == 1) {
        /* Flush write buffer to disk */
        fat32_create_file(f->path, f->write_buf, f->size);
        kfree(f->write_buf);
    }
    kfree(f);
    return 0;
}

int remove(const char* filename) {
    (void)filename;
    return -1;
}

int rename(const char* oldname, const char* newname) {
    (void)oldname;
    (void)newname;
    return -1;
}

/* --- New Stubs for fcntl/unistd --- */

int open(const char *pathname, int flags, ...) {
    (void)pathname; (void)flags;
    return -1;
}

int close(int fd) {
    (void)fd;
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

ssize_t write(int fd, const void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return -1;
}

int unlink(const char *pathname) {
    (void)pathname;
    return -1;
}

unsigned int posix_sleep(unsigned int seconds) {
    (void)seconds;
    return 0;
}

int usleep(unsigned int usec) {
    (void)usec;
    return 0;
}

int access(const char *pathname, int mode) {
    (void)mode;
    
    void* d; size_t s;
    if (doom_iwad_open(pathname, &d, &s)) return 0;

    fat_automount();
    int32_t sz = fat32_get_file_size(pathname);
    return (sz >= 0) ? 0 : -1;
}

int isatty(int fd) {
    (void)fd;
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tv; (void)tz;
    return -1;
}

/* --- Stubs for stdio/stdlib --- */

int errno = 0;

int putchar(int c) {
    terminal_putchar((char)c);
    return c;
}

int vfprintf(FILE* stream, const char* format, va_list arg) {
    (void)stream;
    /* Log to serial for debug */
    char buf[256];
    vsnprintf(buf, 256, format, arg);
    serial("[STDIO] %s", buf);
    terminal_vprintf(format, &arg);
    return 0;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    return ret;
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, 0x7FFFFFFF, format, args);
    va_end(args);
    return ret;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

int system(const char* command) {
    (void)command;
    return -1; /* Indică eșec (shell indisponibil) */
}

/* Global jump buffer for exit() recovery */
jmp_buf exit_jmp_buf;
int exit_jmp_set = 0;

void exit(int status) {
    serial("\n[PROGRAM EXITED] Status: %d\n", status);
    terminal_printf("\n[PROGRAM EXITED] Status: %d\n", status);
    
    if (exit_jmp_set) {
        longjmp(exit_jmp_buf, 1);
    } else {
        /* Fallback if no recovery point set */
        terminal_writestring("System Halted.\n");
        while(1) { asm volatile("hlt"); }
    }
}

/* Minimal vsnprintf implementation */
int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    if (!str || size == 0) return 0;
    
    size_t written = 0;
    const char* p = format;
    
    while (*p && written < size - 1) {
        if (*p != '%') {
            str[written++] = *p++;
            continue;
        }
        
        p++; // skip %
        
        /* Parse flags, width, precision (minimal support for %.3d) */
        int pad_zero = 0;
        int width = 0;
        int precision = -1;
        
        while (*p == '0') { pad_zero = 1; p++; }
        
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') {
                precision = precision * 10 + (*p - '0');
                p++;
            }
        }
        
        if (*p == 0) break;
        
        if (*p == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && written < size - 1) str[written++] = *s++;
        } else if (*p == 'd' || *p == 'i') {
            int v = va_arg(ap, int);
            char buf[32];
            // Simple itoa
            int i = 0;
            int neg = 0;
            if (v < 0) { neg = 1; v = -v; }
            if (v == 0) buf[i++] = '0';
            while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
            
            /* Handle precision (min digits) - acts like zero padding */
            while (i < precision) buf[i++] = '0';
            
            if (neg) buf[i++] = '-';
            while (i > 0 && written < size - 1) str[written++] = buf[--i];
        } else if (*p == 'u') {
            unsigned int v = va_arg(ap, unsigned int);
            char buf[32];
            int i = 0;
            if (v == 0) buf[i++] = '0';
            while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
            while (i > 0 && written < size - 1) str[written++] = buf[--i];
        } else if (*p == 'x' || *p == 'X') {
            unsigned int v = va_arg(ap, unsigned int);
            char buf[32];
            int i = 0;
            const char* hex = (*p == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            if (v == 0) buf[i++] = '0';
            while (v) { buf[i++] = hex[v % 16]; v /= 16; }
            while (i > 0 && written < size - 1) str[written++] = buf[--i];
        } else if (*p == 'c') {
            char c = (char)va_arg(ap, int);
            str[written++] = c;
        } else {
            str[written++] = *p;
        }
        p++;
    }
    str[written] = 0;
    return written;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

/* Minimal sscanf implementation (supports %d, %i, %x) */
int sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int count = 0;
    const char* p = format;
    const char* s = str;
    
    while (*p && *s) {
        if (*p == ' ') { p++; continue; }
        if (*s == ' ') { s++; continue; }
        
        if (*p != '%') {
            if (*p != *s) break;
            p++; s++;
            continue;
        }
        
        p++; // skip %
        if (*p == 0) break;
        
        if (*p == 'd' || *p == 'i') {
            int* ptr = va_arg(args, int*);
            int val = 0;
            int sign = 1;
            if (*s == '-') { sign = -1; s++; }
            else if (*s == '+') s++;
            
            if (!(*s >= '0' && *s <= '9')) break;
            
            while (*s >= '0' && *s <= '9') {
                val = val * 10 + (*s - '0');
                s++;
            }
            *ptr = val * sign;
            count++;
        } else if (*p == 'x' || *p == 'X') {
            int* ptr = va_arg(args, int*);
            unsigned int val = 0;
            
            // Skip 0x if present
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
            
            while (1) {
                int digit = -1;
                if (*s >= '0' && *s <= '9') digit = *s - '0';
                else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
                else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
                
                if (digit == -1) break;
                val = val * 16 + digit;
                s++;
            }
            *ptr = (int)val;
            count++;
        } else {
            // Unsupported
            break;
        }
        p++;
    }
    
    va_end(args);
    return count;
}


/* --- Math / Stdlib Stubs --- */

int abs(int j) {
    return (j < 0) ? -j : j;
}

double atof(const char* s) {
    double res = 0.0;
    int sign = 1;
    if (!s) return 0.0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    
    while (*s >= '0' && *s <= '9') {
        res = res * 10.0 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        double factor = 0.1;
        while (*s >= '0' && *s <= '9') {
            res += (*s - '0') * factor;
            factor *= 0.1;
            s++;
        }
    }
    return res * sign;
}

double pow(double x, double y) {
    (void)y;
    return x; /* Fallback: returnează x (gamma liniar) */
}

double sqrt(double x) {
    return x; /* Aproximare grosolană pentru a evita crash */
}

double fabs(double x) {
    return (x < 0) ? -x : x;
}

double sin(double x) { (void)x; return 0; }
double cos(double x) { (void)x; return 0; }
double floor(double x) { return (int)x; }
double ceil(double x) { return (int)x + 1; }
double atan(double x) { (void)x; return 0; }
double atan2(double y, double x) { (void)y; (void)x; return 0; }
double log(double x) { (void)x; return 0; }
double exp(double x) { (void)x; return 0; }
/* Minimal stdio implementations for freestanding kernel */
#include "include/stdio.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* Simple sprintf implementation */
static int simple_sprintf(char* str, size_t size, const char* fmt, va_list ap, int use_size) {
    char* p = str;
    const char* f = fmt;
    int written = 0;
    int max_write = use_size ? (int)size - 1 : 999999;

    while (*f && written < max_write) {
        if (*f == '%' && *(f + 1)) {
            f++;
            if (*f == 'd' || *f == 'i') {
                int val = va_arg(ap, int);
                char buf[32];
                int i = 0;
                if (val < 0) {
                    if (written < max_write) { *p++ = '-'; written++; }
                    val = -val;
                }
                int temp = val;
                do { i++; temp /= 10; } while (temp);
                temp = val;
                while (i > 0) {
                    buf[--i] = '0' + (temp % 10);
                    temp /= 10;
                }
                for (int j = 0; j < i + 1 && written < max_write; j++, p++, written++) {
                    *p = buf[j];
                }
            } else if (*f == 'x') {
                unsigned val = va_arg(ap, unsigned);
                const char* hex = "0123456789abcdef";
                char buf[16];
                int i = 0;
                if (val == 0) buf[i++] = '0';
                else {
                    unsigned temp = val;
                    while (temp) { temp >>= 4; i++; }
                    temp = val;
                    while (i > 0) {
                        buf[--i] = hex[temp & 0xf];
                        temp >>= 4;
                    }
                    i = 0;
                    while (buf[i]) {
                        if (written < max_write) { *p++ = buf[i]; written++; }
                        i++;
                    }
                }
            } else if (*f == 's') {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                while (*s && written < max_write) {
                    *p++ = *s++;
                    written++;
                }
            } else if (*f == 'c') {
                int c = va_arg(ap, int);
                if (written < max_write) {
                    *p++ = (char)c;
                    written++;
                }
            } else if (*f == '%') {
                if (written < max_write) {
                    *p++ = '%';
                    written++;
                }
            }
            f++;
        } else {
            *p++ = *f++;
            written++;
        }
    }

    if (written <= max_write) *p = '\0';
    else if (use_size && size > 0) str[size - 1] = '\0';
    return written;
}

int sprintf(char* str, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = simple_sprintf(str, 9999, fmt, ap, 0);
    va_end(ap);
    return result;
}

int snprintf(char* str, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = simple_sprintf(str, size, fmt, ap, 1);
    va_end(ap);
    return result;
}

int vsnprintf(char* str, size_t size, const char* fmt, va_list ap) {
    return simple_sprintf(str, size, fmt, ap, 1);
}

int fprintf(FILE* stream, const char* fmt, ...) {
    /* Stub for now - just return success */
    (void)stream;
    (void)fmt;
    return 0;
}

/* File I/O stubs - for freestanding, these are minimal */
FILE* fopen(const char* filename, const char* mode) {
    (void)filename;
    (void)mode;
    return (FILE*)1;  /* Return non-null stub */
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;  /* Return 0 items read */
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return nmemb;  /* Pretend write succeeded */
}

int fseek(FILE* stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return 0;
}

long ftell(FILE* stream) {
    (void)stream;
    return 0;
}

int fclose(FILE* stream) {
    (void)stream;
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

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

/* Add timer_get_ticks function */
extern volatile uint64_t ticks;  /* from timer.c */

uint32_t timer_get_ticks(void) {
    /* Return lower 32 bits of ticks */
    extern uint64_t timer_ticks(void);
    return (uint32_t)timer_ticks();
}

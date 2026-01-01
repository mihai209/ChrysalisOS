#include "string.h"

size_t strlen(const char* s)
{
    size_t i = 0;
    while (s[i])
        i++;
    return i;
}

int strcmp(const char* a, const char* b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == 0 || b[i] == 0)
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

char* strcpy(char* dst, const char* src)
{
    char* r = dst;
    while ((*dst++ = *src++))
        ;
    return r;
}

char* strncpy(char* dst, const char* src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];

    for (; i < n; i++)
        dst[i] = 0;

    return dst;
}

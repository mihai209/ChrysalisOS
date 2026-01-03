#include <stdint.h>
#include <stdbool.h>

bool user_ptr_valid(const void* ptr)
{
    return ((uint32_t)ptr < 0xC0000000);
}

int copy_from_user(void* dst, const void* src, uint32_t size)
{
    if (!user_ptr_valid(src))
        return -1;

    const uint8_t* s = src;
    uint8_t* d = dst;

    for (uint32_t i = 0; i < size; i++)
        d[i] = s[i];

    return 0;
}

int copy_to_user(void* dst, const void* src, uint32_t size)
{
    if (!user_ptr_valid(dst))
        return -1;

    const uint8_t* s = src;
    uint8_t* d = dst;

    for (uint32_t i = 0; i < size; i++)
        d[i] = s[i];

    return 0;
}

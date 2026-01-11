#pragma once
#include <stddef.h>
void kmalloc_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kmalloc_aligned(size_t size, size_t alignment);

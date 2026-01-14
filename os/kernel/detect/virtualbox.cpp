#include "virtualbox.h"
#include "../panic.h"

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
        : "memory"
    );
}

void virtualbox_check_or_panic(void)
{
    uint32_t eax, ebx, ecx, edx;

    /* CPUID leaf 1: Hypervisor present bit */
    cpuid(0x1, 0, &eax, &ebx, &ecx, &edx);

    if ((ecx & (1u << 31)) == 0)
        return;

    /* CPUID leaf 0x40000000: Hypervisor vendor ID */
    cpuid(0x40000000, 0, &eax, &ebx, &ecx, &edx);

    /* "VBoxVBoxVBox" */
    if (ebx == 0x786f6256 &&
        ecx == 0x786f6256 &&
        edx == 0x786f6256)
    {
        panic("ChrysalisOS: VirtualBox detected. Execution halted to prevent corruption.\n");
    }
}

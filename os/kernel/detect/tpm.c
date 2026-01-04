// kernel/detect/tpm.c
#include "tpm.h"

#include "../terminal.h"
#include "../panic.h"

#define TPM_BASE_PHYS 0xFED40000u
#define TPM_DID_VID   0xF00u

/* TPM DID_VID register (TCG TIS) */
struct tpm_did_vid {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  revision;
    uint8_t  interface_rev;
    uint16_t reserved;
} __attribute__((packed));

static inline uint32_t mmio_read32(uintptr_t addr)
{
    volatile uint32_t *p = (volatile uint32_t *)addr;
    return *p;
}

int tpm_detect_v12(void)
{
    uintptr_t reg = TPM_BASE_PHYS + TPM_DID_VID;

    uint32_t raw = mmio_read32(reg);

    /* no device mapped */
    if (raw == 0xFFFFFFFFu || raw == 0x00000000u)
        return 0;

    struct tpm_did_vid *id = (struct tpm_did_vid *)&raw;

    /* TPM 1.2 requires interface revision = 1 */
    if (id->interface_rev != 1)
        return 0;

    return 1;
}

void tpm_check_or_panic(void)
{
    terminal_writestring("[tpm] probing TPM 1.2...\n");

    if (!tpm_detect_v12()) {
        panic(
            "TPM 1.2 REQUIRED\n"
            "----------------\n"
            "No compatible TPM detected.\n"
            "Chrysalis OS requires TPM v1.2 to boot.\n\n"
            "Notes:\n"
            "- Enable TPM in BIOS/UEFI\n"
            "- Use QEMU with swtpm\n"
            "- TPM 2.0 is NOT supported yet"
        );
    }

    terminal_writestring("[tpm] TPM 1.2 detected OK\n");
}

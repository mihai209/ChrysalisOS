// kernel/detect/tpm.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* returns 1 if TPM 1.2 present, 0 otherwise */
int tpm_detect_v12(void);

/* panics if TPM 1.2 is missing */
void tpm_check_or_panic(void);

#ifdef __cplusplus
}
#endif

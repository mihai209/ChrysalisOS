#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pic_remap(int offset1, int offset2);
void pic_send_eoi(unsigned char irq);

#ifdef __cplusplus
}
#endif

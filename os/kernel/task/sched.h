#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void schedule(void); /* pick next ready task and switch_to */

#ifdef __cplusplus
}
#endif

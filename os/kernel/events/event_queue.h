#pragma once
#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

void event_queue_init(void);

int event_push(const event_t* ev);
int event_pop(event_t* out);

int event_queue_is_empty(void);

#ifdef __cplusplus
}
#endif

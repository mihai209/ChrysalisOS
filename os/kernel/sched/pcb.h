#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TASKS 16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE
} task_state_t;

/* Primary task function type used by the scheduler implementation:
 * takes a single void* argument (can be NULL).
 */
typedef void (*task_fn_t)(void* arg);

/* For convenience, also define the common no-arg form */
typedef void (*task_fn_noarg_t)(void);

typedef struct {
    uint32_t *esp;                /* saved stack pointer (for context switch) */
    uint8_t stack[TASK_STACK_SIZE];
    task_state_t state;
    uint32_t tid;
    task_fn_t entry;
    void* arg;
    uint32_t ticks_remaining;     /* quantum remaining (in ticks) */
} pcb_t;

/* API used by scheduler (C linkage) */
void pcb_init_all(void);
int pcb_create(task_fn_t entry, void* arg);
pcb_t* pcb_get_current(void);

#ifdef __cplusplus
} /* extern "C" */

/* ----------------------
   C++ convenience overloads
   ----------------------
   kernel/kernel.cpp is compiled as C++; it often defines tasks as
     void task_a(void);
   but the scheduler implementation expects void(*)(void*).
   To make calling pcb_create(task_a, NULL) work without casts we
   provide small C++ wrappers that forward/cast to the C API.
   These wrappers are only available in C++ translation units.
*/
inline int pcb_create(task_fn_noarg_t entry, void* arg) {
    return pcb_create((task_fn_t)entry, arg);
}

/* If you prefer calling without providing arg (NULL by default): */
inline int pcb_create(task_fn_noarg_t entry) {
    return pcb_create(entry, (void*)0);
}

#endif /* __cplusplus */

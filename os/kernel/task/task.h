#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_NAME_LEN 16
#define TASK_STACK_BYTES 4096

enum task_state {
    TASK_RUNNING = 0,
    TASK_READY   = 1,
    TASK_ZOMBIE  = 2
};

typedef struct task {
    uint32_t esp;             /* stored kernel stack pointer (saved by switch_to) */
    uint32_t pid;
    uint8_t  state;
    struct task *next;        /* circular list for scheduler */
    char name[TASK_NAME_LEN];
    void *stack_base;         /* pointer returned by allocator (for free) */
} task_t;

/* public API */
extern task_t *current_task;

void task_init(void);                          /* init subsystem, capture main task */
task_t *task_create(const char *name, void (*entry)(void));
void task_yield(void);                         /* cooperative yield */

#ifdef __cplusplus
}
#endif

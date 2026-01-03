/* kernel/task/sched.c */
#include "sched.h"
#include "task.h"

/* assembly context switch */
extern void switch_to(task_t *prev, task_t *next);

void schedule(void)
{
    if (!current_task) return;

    /* trivial round-robin: pick next ready task */
    task_t *prev = current_task;
    task_t *next = current_task->next;

    /* find next runnable (simple) */
    while (next && next != current_task && next->state != TASK_READY) {
        next = next->next;
    }

    if (!next || next == current_task) {
        /* nothing to do */
        return;
    }

    /* mark states */
    prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current_task = next;

    /* context switch: save prev->esp, load next->esp */
    switch_to(prev, next);

    /* when we return here, we've been switched back to this task */
}

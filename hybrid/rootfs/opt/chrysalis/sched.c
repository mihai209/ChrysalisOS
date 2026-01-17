#include "task.h"
extern void context_switch(void *prev_ptr_addr, void *next_ptr_addr);

void task_init_scheduler(void) {
    /* creează 2 task-uri de test (exemplu) */
    extern void task_function_a(void);
    extern void task_function_b(void);

    task_create(task_function_a, 1);
    task_create(task_function_b, 2);

    current_task = task_list; /* primul task */
}

/* simplu round-robin: current_task->next */
void schedule(void) {
    if (!current_task) return;
    task_t *prev = current_task;
    task_t *next = current_task->next;

    if (next == prev) return;

    /* apel assembly: context_switch(&prev->kstack_ptr, &next->kstack_ptr) */
    context_switch(&prev->kstack_ptr, &next->kstack_ptr);
    current_task = next;
}

/* forțează switch imediat */
void yield(void) {
    schedule();
}

/* Ex: simple test tasks */
void task_function_a(void) {
    for (;;) {
        /* ceva I/O de test, ex terminal_write */
        // terminal_writestring("A\n");
        for (volatile int i=0;i<100000;i++); /* delay */
        yield();
    }
}

void task_function_b(void) {
    for (;;) {
        // terminal_writestring("B\n");
        for (volatile int i=0;i<100000;i++); /* delay */
        yield();
    }
}

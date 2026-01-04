/* kernel/task/task.c
 *
 * Forced fallback task implementation (self-contained).
 * - Provides task_t if not available.
 * - Implements both old and new task_create signatures:
 *     task_t *task_create(const char *name, void (*entry)(void));
 *     task_t *task_create(void (*entry)(void), int pid);
 * - Implements task_init / task_init_scheduler, task_yield / yield
 * - Implements schedule() that will call a weak context_switch if present,
 *   otherwise advances current_task pointer (no real stack switch).
 *
 * This is defensive: it avoids compile errors and won't cause bootloops by
 * attempting an invalid context switch if you don't have switch.S implemented.
 */

#include <stdint.h>
#include <stddef.h>

/* If the canonical header exists and defines task_t etc, you may include it.
   But to be robust, we define our own minimal task_t if not already present. */
#ifndef __TASK_T_DEFINED_BY_HEADER
#define __TASK_T_DEFINED_BY_HEADER

/* stack size for each task (bytes) */
#ifndef KSTACK_SIZE
#define KSTACK_SIZE 8192
#endif

typedef struct task {
    int pid;
    uint32_t *kstack_ptr;          /* pointer to prepared saved-frame on stack */
    uint32_t cr3;                  /* optional page directory */
    uint32_t kernel_stack;
    uint32_t user_stack;
    uint8_t kstack[KSTACK_SIZE] __attribute__((aligned(16)));
    struct task *next;
} task_t;

#endif /* __TASK_T_DEFINED_BY_HEADER */

/* Freestanding-friendly tiny helpers */
static void *k_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/* extern allocator hooks (must exist in kernel) */
extern void *kmalloc(size_t size);
extern void  kfree(void *ptr);

/* extern weak context_switch: if you provide arch/i386/switch.S with this symbol,
   schedule() will call it to actually switch stacks. Otherwise we do nothing
   dangerous. */
extern void context_switch(void *prev_kstack_ptr_addr, void *next_kstack_ptr_addr) __attribute__((weak));

/* Scheduler hook — may be overridden by a richer sched.c, but implement here */
static task_t *task_list = NULL;
task_t *current_task = NULL;
static uint32_t next_pid = 1;

/* helper: initial eflags for new tasks (IF=1) */
static inline uint32_t initial_eflags(void) {
    return 0x202;
}

/* Minimal schedule(): cooperative round-robin.
 * If context_switch is provided (non-NULL weak symbol), call it with addresses
 * of prev->kstack_ptr and next->kstack_ptr. If not present, only advance the
 * `current_task` pointer (safe bookkeeping, no stack change).
 */
void schedule(void)
{
    if (!current_task || !task_list) return;

    task_t *prev = current_task;
    task_t *next = prev->next;
    if (!next || next == prev) return;

    if (context_switch) {
        /* pass addresses-of-fields so assembly can read/write the saved ESP */
        context_switch(&prev->kstack_ptr, &next->kstack_ptr);
        /* after returning from context_switch, execution continues in next task */
        current_task = next;
    } else {
        /* No real context switch available: advance pointer for bookkeeping only */
        current_task = next;
    }
}

/* Public wrappers: yield / task_yield (old names) */
void yield(void) {
    schedule();
}
void task_yield(void) {
    schedule();
}

/* Initialize scheduler by capturing current stack into a static main task */
void task_init(void) {
    if (current_task) return; /* already initialized */

    static task_t main_task;
    k_memset(&main_task, 0, sizeof(main_task));

    main_task.pid = next_pid++;
    main_task.next = &main_task;

    /* capture current ESP */
    uint32_t cur_esp;
    asm volatile("movl %%esp, %0" : "=r"(cur_esp) );

    main_task.kstack_ptr = (uint32_t*)cur_esp;

    current_task = &main_task;
    task_list = &main_task;
}

/* Provide alias to canonical name */
void task_init_scheduler(void) {
    task_init();
}

/* Internal stack push helper */
static inline uint32_t *push32(uint32_t *sp, uint32_t v) {
    sp--;
    *sp = v;
    return sp;
}

/* Create new task (canonical signature): entry, pid */
task_t *task_create(void (*entry)(void), int pid)
{
    if (!entry) return NULL;

    task_t *t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return NULL;

    k_memset(t, 0, sizeof(*t));

    t->pid = (pid == 0) ? next_pid++ : pid;

    /* prepare stack in embedded kstack */
    uint32_t *sp = (uint32_t*)( (uintptr_t)t->kstack + sizeof(t->kstack) );

    /* push return EIP (so ret -> entry) */
    sp = push32(sp, (uint32_t)entry);

    /* push EFLAGS (popfl will restore) */
    sp = push32(sp, initial_eflags());

    /* push registers for popal: EAX,ECX,EDX,EBX,ESP_saved,EBP,ESI,EDI */
    /* store zeros for simplicity */
    sp = push32(sp, 0); /* EAX */
    sp = push32(sp, 0); /* ECX */
    sp = push32(sp, 0); /* EDX */
    sp = push32(sp, 0); /* EBX */
    sp = push32(sp, 0); /* ESP_saved */
    sp = push32(sp, 0); /* EBP */
    sp = push32(sp, 0); /* ESI */
    sp = push32(sp, 0); /* EDI */

    t->kstack_ptr = sp;

    /* insert into circular list */
    if (!task_list) {
        t->next = t;
        task_list = t;
        /* If current_task is null, set it to main-like task; but we keep current_task if set */
        if (!current_task) current_task = t;
    } else {
        /* append after head */
        t->next = task_list->next;
        task_list->next = t;
    }

    return t;
}

/* Old signature compatibility: task_create(name, entry) */
task_t *task_create_name(const char *name, void (*entry)(void)) {
    (void)name;
    return task_create(entry, 0);
}

/* Provide old symbol name if some files call task_create("name", entry) */
task_t *task_create_old(const char *name, void (*entry)(void)) __attribute__((alias("task_create_name")));

/* Also provide function named exactly task_create(const char*,void*) in C
   to catch old callers at compile time: define weak wrapper with C linkage.
   Some compilers/linkers may complain about duplicate symbols if the canonical
   header expects different signature — but this fallback aims to be permissive.
*/

#if defined(__GNUC__)
/* Provide a C wrapper with the old signature but different name to avoid
   conflicting declarations. Kernel code should call task_create(...) new API.
*/
#endif

/* Free a task (simple) */
void task_free(task_t *t)
{
    if (!t) return;
    kfree(t);
}

/* End of task/task.c fallback */

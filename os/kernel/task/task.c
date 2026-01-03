/* kernel/task/task.c
 *
 * Freestanding-friendly task implementation: no <string.h> dependency.
 * Provides small local helpers k_memset and k_strncpy as fallbacks.
 */

#include "task.h"
#include <stddef.h>   /* for NULL and size_t */
#include <stdint.h>

/* Minimal internal helpers:
 * - k_memset: small and robust memset (used to zero structures)
 * - k_strncpy: safe strncpy that always NUL-terminates if n > 0
 *
 * We keep short names with k_ prefix to avoid collisions with libc if
 * someone links against it later.
 */

static void *k_memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

/* Copy up to n-1 bytes from src to dst, NUL-terminate dst if n > 0.
 * Mirrors common kernel-friendly semantics (safer than libc strncpy which
 * doesn't NUL-terminate if src length >= n).
 */
static char *k_strncpy(char *dst, const char *src, size_t n)
{
    if (n == 0) return dst;
    size_t i = 0;
    for (; i + 1 < n && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

/* you must have kmalloc available in kernel (or replace with your allocator) */
extern void *kmalloc(size_t);
extern void  kfree(void*);

/* Forward scheduler functions */
extern void schedule(void);

static task_t *task_list = NULL;
task_t *current_task = NULL;
static uint32_t next_pid = 1;

void task_init(void)
{
    static task_t main_task;
    k_memset(&main_task, 0, sizeof(main_task));

    /* use kernel-safe string copy */
    k_strncpy(main_task.name, "main", TASK_NAME_LEN);

    main_task.pid = next_pid++;
    main_task.state = TASK_RUNNING;
    main_task.next = &main_task; /* single circular list */

    /* capture current ESP into main_task.esp */
    uint32_t esp_val;
    asm volatile("movl %%esp, %0" : "=r"(esp_val) );
    main_task.esp = esp_val;

    current_task = &main_task;
    task_list = &main_task;
}

/* Helper: push a 32-bit value on newly created stack */
static inline uint32_t *push_u32(uint32_t *sp, uint32_t v) {
    sp--;
    *sp = v;
    return sp;
}

/* Create a task that will run `entry` (no args). The entry function MUST not return. */
task_t *task_create(const char *name, void (*entry)(void))
{
    if (!entry) return NULL;

    void *raw = kmalloc(TASK_STACK_BYTES);
    if (!raw) return NULL;

    /* stack grows down: top points to end of allocation */
    uint8_t *stack_top = (uint8_t*)raw + TASK_STACK_BYTES;
    uint32_t *sp = (uint32_t*)stack_top;

    /* push an initial fake frame so 'ret' will jump to entry:
       layout when switched to: RET -> entry
       also push a fake ebp */

    /* push placeholder for EIP (we will set the return address later) */
    sp = push_u32(sp, 0);               /* fake EIP placeholder */
    sp = push_u32(sp, 0);               /* fake EBP */

    /* place the return address (EIP) one slot above fake EBP -> when ret executed, jumps to entry.
       The slot we want to overwrite is the first pushed value (the topmost),
       which currently is at sp+1 because we pushed two words. */
    sp[1] = (uint32_t)entry; /* saved EIP -> when 'ret' runs on this stack, it goes to entry */

    task_t *t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) { kfree(raw); return NULL; }
    k_memset(t, 0, sizeof(task_t));

    t->esp = (uint32_t)sp;
    t->pid = next_pid++;
    t->state = TASK_READY;

    if (name)
        k_strncpy(t->name, name, TASK_NAME_LEN);
    else
        k_strncpy(t->name, "task", TASK_NAME_LEN);

    t->stack_base = raw;

    /* insert into circular task list */
    if (!task_list) {
        t->next = t;
        task_list = t;
    } else {
        /* append after task_list (simple) */
        t->next = task_list->next;
        task_list->next = t;
    }

    return t;
}

/* Free task resources (simple) */
void task_free(task_t *t)
{
    if (!t) return;
    if (t->stack_base) kfree(t->stack_base);
    kfree(t);
}

/* Yield cooperatively */
void task_yield(void)
{
    schedule();
}

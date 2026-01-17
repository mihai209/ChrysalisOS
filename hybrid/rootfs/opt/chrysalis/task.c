#include "task.h"
#include "paging.h" /* opțional: pentru cr3 */
#include <stddef.h>
#include <string.h>

task_t *current_task = NULL;
static task_t *task_list = NULL;

/* valori implicite regs pentru un task nou */
static inline uint32_t initial_eflags(void) {
    return 0x202; /* IF = 1, bit interrupt enable */
}

/* Creează un task simplu (kernel thread) care pornește la `entry`. */
task_t *task_create(void (*entry)(void), int pid) {
    task_t *t = /* kmalloc sau static allocate; exemplu simplu: */ NULL;

    /* pentru exemplu folosesc kmalloc simplu (presupun că ai kmalloc) */
    extern void *kmalloc(size_t);
    t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(*t));

    t->pid = pid;
    t->cr3 = 0; /* 0 înseamnă folosește cr3 curent; setează dacă ai proc. virtuale */

    /* Pregătim stiva dinspre capătul superior */
    uint32_t *sp = (uint32_t*)( (uintptr_t)t->kstack + KSTACK_SIZE );

    /* Push: RETURN_EIP */
    *(--sp) = (uint32_t)entry;        /* ret va sări la entry când taskul e activat */

    /* Push: EFLAGS (valoare pentru popfl) */
    *(--sp) = initial_eflags();

    /* Push registers în ordinea care face ca popal să le populeze corect:
       popal va popa: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX  (în această ordine). */
    *(--sp) = 0xDEADBEEF; /* EAX */
    *(--sp) = 0xCAFEBABE; /* ECX */
    *(--sp) = 0xFEEDFACE; /* EDX */
    *(--sp) = 0;          /* EBX */
    *(--sp) = 0;          /* ESP_saved (valori arbitrare) */
    *(--sp) = 0;          /* EBP */
    *(--sp) = 0;          /* ESI */
    *(--sp) = 0;          /* EDI */

    t->kstack_ptr = sp; /* pointer la începutul frame-ului pregătit */
    t->next = NULL;

    /* adaugă la lista de taskuri (circular) */
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        task_t *it = task_list;
        while (it->next != task_list) it = it->next;
        it->next = t;
        t->next = task_list;
    }

    return t;
}

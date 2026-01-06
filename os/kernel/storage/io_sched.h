#ifndef IO_SCHED_H
#define IO_SCHED_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IO_OP_READ,
    IO_OP_WRITE
} io_op_t;

typedef void (*io_callback_t)(int status, void *ctx);

/* Adaugă o cerere în coadă (non-blocking) */
int io_sched_submit(int port, io_op_t op, uint64_t lba, uint32_t count, void *buf, io_callback_t cb, void *ctx);

/* Procesează coada (trebuie apelat în main loop sau timer interrupt) */
void io_sched_poll(void);

/* Inițializează scheduler-ul */
void io_sched_init(void);

#ifdef __cplusplus
}
#endif

#endif
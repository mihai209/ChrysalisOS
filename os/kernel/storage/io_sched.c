#include "io_sched.h"
#include "ahci/ahci.h"

#define MAX_IO_REQUESTS 64

typedef struct {
    int active;
    int port;
    io_op_t op;
    uint64_t lba;
    uint32_t count;
    void *buf;
    io_callback_t cb;
    void *ctx;
} io_request_t;

static io_request_t queue[MAX_IO_REQUESTS];
static int head = 0;
static int tail = 0;

void io_sched_init(void) {
    for (int i = 0; i < MAX_IO_REQUESTS; i++) {
        queue[i].active = 0;
    }
}

int io_sched_submit(int port, io_op_t op, uint64_t lba, uint32_t count, void *buf, io_callback_t cb, void *ctx) {
    int next = (head + 1) % MAX_IO_REQUESTS;
    if (next == tail) return -1; // Queue full

    io_request_t *req = &queue[head];
    req->port = port;
    req->op = op;
    req->lba = lba;
    req->count = count;
    req->buf = buf;
    req->cb = cb;
    req->ctx = ctx;
    req->active = 1;

    head = next;
    return 0;
}

void io_sched_poll(void) {
    if (head == tail) return; // Empty

    io_request_t *req = &queue[tail];
    if (req->active) {
        int status = -1;
        // Dispatch to driver (AHCI)
        // Nota: Deocamdată AHCI e sincron, deci asta va bloca scurt timp.
        // Pe viitor, ahci_read_lba ar trebui să returneze imediat și să seteze un flag.
        if (req->op == IO_OP_READ) {
            status = ahci_read_lba(req->port, req->lba, req->count, req->buf);
        } else {
            status = ahci_write_lba(req->port, req->lba, req->count, req->buf);
        }

        if (req->cb) {
            req->cb(status, req->ctx);
        }
        req->active = 0;
    }
    tail = (tail + 1) % MAX_IO_REQUESTS;
}
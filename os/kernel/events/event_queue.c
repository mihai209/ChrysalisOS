#include "event_queue.h"

#define EVENT_QUEUE_SIZE 64

static event_t queue[EVENT_QUEUE_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

void event_queue_init(void)
{
    head = 0;
    tail = 0;
}

static int next_index(int i)
{
    return (i + 1) % EVENT_QUEUE_SIZE;
}

int event_queue_is_empty(void)
{
    return head == tail;
}

int event_push(const event_t* ev)
{
    int next = next_index(head);

    if (next == tail) {
        // queue full → drop event
        return -1;
    }

    queue[head] = *ev;
    head = next;
    return 0;
}

int event_pop(event_t* out)
{
    if (head == tail)
        return -1;

    *out = queue[tail];
    tail = next_index(tail);
    return 0;
}

#ifndef NEXIX_NET_SCHED_H
#define NEXIX_NET_SCHED_H
#include "net/types.h"

/* Allocation-free work queue and protocol timer list for the network worker
 * (task 415). The caller serializes queue operations (IRQs disabled in the
 * kernel); timers are owned by the worker alone.
 */

/* Intrusive work item, embedded in its owner. Queueing an item that is still
 * pending coalesces instead of duplicating it, as BSD schednetisr does.
 */
struct nx_net_work {
    struct nx_net_work *next;
    void (*function)(void *);
    void *argument;
    bool queued;
};
struct nx_net_queue {
    struct nx_net_work *head, *tail;
    size_t count, limit;
    uint64_t enqueued, coalesced, overflows, taken;
};
enum nx_net_put { NX_NET_PUT_QUEUED, NX_NET_PUT_PENDING, NX_NET_PUT_FULL };
void nx_net_work_init(struct nx_net_work *, void (*function)(void *), void *argument);
void nx_net_queue_init(struct nx_net_queue *, size_t limit);
/* A full queue refuses the item and counts an overflow; the producer drops. */
enum nx_net_put nx_net_queue_put(struct nx_net_queue *, struct nx_net_work *);
/* The returned item is no longer queued and may be queued again. */
struct nx_net_work *nx_net_queue_take(struct nx_net_queue *);
/* Remove a pending item; false when it was not queued. */
bool nx_net_queue_remove(struct nx_net_queue *, struct nx_net_work *);

/* One-shot timer at an absolute 64-bit tick deadline. BSD protocol timers
 * (pr_fasttimo/pr_slowtimo countdowns and TCPT_* slots) become individual
 * timers; periodic users rearm from their callback.
 */
struct nx_net_timer {
    struct nx_net_timer *next;
    uint64_t deadline;
    void (*function)(void *);
    void *argument;
    bool pending;
};
struct nx_net_timers {
    struct nx_net_timer *head;   /* Sorted by deadline, FIFO among equals. */
    uint64_t armed, cancelled, expired;
};
void nx_net_timer_init(struct nx_net_timer *, void (*function)(void *), void *argument);
void nx_net_timers_init(struct nx_net_timers *);
/* Arm or rearm at now + delay (saturating). A pending timer moves. */
void nx_net_timer_arm(struct nx_net_timers *, struct nx_net_timer *, uint64_t now, uint64_t delay);
/* After a successful cancel the callback will not run for that arming. */
bool nx_net_timer_cancel(struct nx_net_timers *, struct nx_net_timer *);
/* Earliest deadline; false when nothing is armed. */
bool nx_net_timers_next(const struct nx_net_timers *, uint64_t *deadline);
/* Expire at most budget due timers in deadline order, each unlinked before its
 * callback runs, so callbacks may rearm or cancel any timer. Returns the count.
 */
size_t nx_net_timers_run(struct nx_net_timers *, uint64_t now, size_t budget);
#endif

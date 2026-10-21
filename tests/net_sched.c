#include <assert.h>
#include <string.h>
#include "net/sched.h"

static unsigned log_[64], logged;
static struct nx_net_timers timers;
static struct nx_net_timer t[8];
static void record(void *argument) { log_[logged++] = (unsigned)(uintptr_t)argument; }

static void queue(void)
{
    struct nx_net_queue q;
    struct nx_net_work w[5];
    nx_net_queue_init(&q, 3);
    for (uintptr_t i = 0; i < 5; ++i) nx_net_work_init(&w[i], record, (void *)i);
    assert(!nx_net_queue_take(&q));
    assert(nx_net_queue_put(&q, &w[0]) == NX_NET_PUT_QUEUED);
    assert(nx_net_queue_put(&q, &w[0]) == NX_NET_PUT_PENDING && q.count == 1);
    assert(nx_net_queue_put(&q, &w[1]) == NX_NET_PUT_QUEUED);
    assert(nx_net_queue_put(&q, &w[2]) == NX_NET_PUT_QUEUED);
    assert(nx_net_queue_put(&q, &w[3]) == NX_NET_PUT_FULL && !w[3].queued);
    assert(q.overflows == 1 && q.coalesced == 1 && q.enqueued == 3);
    /* Remove from the middle and the tail; FIFO order survives. */
    assert(nx_net_queue_remove(&q, &w[1]) && !nx_net_queue_remove(&q, &w[1]));
    assert(nx_net_queue_remove(&q, &w[2]) && q.tail == &w[0]);
    assert(!nx_net_queue_remove(&q, &w[4]));
    assert(nx_net_queue_put(&q, &w[3]) == NX_NET_PUT_QUEUED);
    assert(nx_net_queue_put(&q, &w[1]) == NX_NET_PUT_QUEUED);
    struct nx_net_work *x;
    unsigned expected[] = {0, 3, 1, 0}, n = 0;
    while ((x = nx_net_queue_take(&q))) {
        assert(!x->queued && x->argument == (void *)(uintptr_t)expected[n++]);
        if (n == 1) assert(nx_net_queue_put(&q, x) == NX_NET_PUT_QUEUED);   /* requeue */
        if (n == 4) break;
    }
    assert(n == 4 && q.count == 0 && !q.head && !q.tail);
    /* An item queued elsewhere is not removed from this queue. */
    struct nx_net_queue other;
    nx_net_queue_init(&other, 1);
    assert(nx_net_queue_put(&other, &w[4]) == NX_NET_PUT_QUEUED);
    assert(!nx_net_queue_remove(&q, &w[4]) && w[4].queued);
    assert(nx_net_queue_remove(&other, &w[4]));
    /* Zero limit refuses everything. */
    nx_net_queue_init(&q, 0);
    assert(nx_net_queue_put(&q, &w[0]) == NX_NET_PUT_FULL);
}

static void rearm(void *argument)
{
    record(argument);
    if (logged < 5) nx_net_timer_arm(&timers, &t[6], 100, 0);   /* due again now */
    assert(nx_net_timer_cancel(&timers, &t[7]));             /* cancel a later due timer */
}
static void timer(void)
{
    uint64_t deadline;
    nx_net_timers_init(&timers);
    for (uintptr_t i = 0; i < 8; ++i) nx_net_timer_init(&t[i], record, (void *)i);
    assert(!nx_net_timers_next(&timers, &deadline) && !nx_net_timer_cancel(&timers, &t[0]));
    nx_net_timer_arm(&timers, &t[0], 10, 5);
    nx_net_timer_arm(&timers, &t[1], 10, 2);
    nx_net_timer_arm(&timers, &t[2], 10, 5);     /* equal deadline: FIFO after t0 */
    nx_net_timer_arm(&timers, &t[3], 10, 1);
    nx_net_timer_arm(&timers, &t[3], 10, 9);     /* rearm moves */
    nx_net_timer_arm(&timers, &t[4], UINT64_MAX - 1, 10);   /* saturates */
    nx_net_timer_arm(&timers, &t[5], 10, 3);
    assert(nx_net_timer_cancel(&timers, &t[5]) && !t[5].pending);
    assert(nx_net_timers_next(&timers, &deadline) && deadline == 12);
    assert(t[4].deadline == UINT64_MAX);
    assert(nx_net_timers_run(&timers, 11, 10) == 0);
    assert(nx_net_timers_run(&timers, 15, 2) == 2);          /* budget bounds work */
    assert(nx_net_timers_run(&timers, 15, 10) == 1);
    assert(nx_net_timers_run(&timers, 100, 10) == 1);
    assert(logged == 4 && log_[0] == 1 && log_[1] == 0 && log_[2] == 2 && log_[3] == 3);
    assert(!t[0].pending && !nx_net_timer_cancel(&timers, &t[0]));
    assert(timers.expired == 4 && timers.cancelled == 1 && timers.armed == 7);

    /* A callback may rearm itself and cancel others. */
    t[6].function = rearm;
    nx_net_timer_arm(&timers, &t[6], 100, 0);
    nx_net_timer_arm(&timers, &t[7], 100, 0);
    assert(nx_net_timers_run(&timers, 100, 1) == 1 && logged == 5 && !t[7].pending);
    assert(nx_net_timer_cancel(&timers, &t[4]) && !timers.head);
    assert(nx_net_timers_run(&timers, UINT64_MAX, 10) == 0);
}

int main(void)
{
    queue();
    logged = 0;
    timer();
    return 0;
}

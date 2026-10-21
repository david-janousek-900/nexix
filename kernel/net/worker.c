#include "net/worker.h"
#include "process.h"
#include <nexix/syscall.h>

static struct nx_net_queue queue;      /* Guarded by disabling IRQs. */
static struct nx_net_timers timers;    /* Worker context only. */
static struct proc *worker;
static bool stopping, stopped, in_callback;
static uint64_t completed, sleeps;
static char channel, stop_channel;

bool net_worker_running(void)
{
    return worker && !stopped;
}
bool net_worker_context(void)
{
    return in_callback && proc_current() == worker;
}
static void require_worker(void)
{
    if (!net_worker_context()) panic("network timer outside the network worker");
}

/* Returns with IRQs disabled once work is queued, a timer is due, or a stop
 * was requested. The scheduler's timed sleep wakes us at the next deadline. */
static void wait_for_work(void)
{
    for (;;) {
        uint64_t deadline;
        bool armed = nx_net_timers_next(&timers, &deadline);
        if (queue.count || stopping || (armed && deadline <= timer_ticks())) return;
        worker->p_deadline = deadline;
        worker->p_timed_sleep = armed;
        ++sleeps;
        proc_sleep(&channel, NX_NET_WORKER_PRIORITY);
        worker->p_timed_sleep = false;
    }
}
static void worker_main(void *argument)
{
    (void)argument;
    for (;;) {
        uintptr_t flags = proc_irq_save();
        wait_for_work();
        if (stopping && !queue.count) {
            while (timers.head) nx_net_timer_cancel(&timers, timers.head);
            stopped = true;
            proc_wakeup(&stop_channel);
            proc_irq_restore(flags);
            proc_exit(0);
        }
        struct nx_net_work *work;
        for (unsigned n = 0; n < NX_NET_WORK_BATCH && (work = nx_net_queue_take(&queue)); ++n) {
            proc_irq_restore(flags);
            in_callback = true;
            work->function(work->argument);
            in_callback = false;
            flags = proc_irq_save();
            ++completed;
        }
        proc_irq_restore(flags);
        in_callback = true;
        nx_net_timers_run(&timers, timer_ticks(), NX_NET_WORK_BATCH);
        in_callback = false;
        proc_yield();   /* Bounded passes keep user processes runnable. */
    }
}

bool net_worker_start(void)
{
    uintptr_t flags = proc_irq_save();
    bool ok = !net_worker_running();
    if (ok) {
        nx_net_queue_init(&queue, NX_NET_WORK_LIMIT);
        nx_net_timers_init(&timers);
        stopping = stopped = false;
        completed = sleeps = 0;
        v7_pid_t pid = proc_spawn(worker_main, NULL);
        worker = NULL;
        const struct proc *table = proc_table();
        for (unsigned i = 0; pid > 0 && i < V7_NPROC && !worker; ++i)
            if (table[i].p_pid == pid && table[i].p_stat != V7_SZOMB) worker = (struct proc *)&table[i];
        ok = pid > 0 && worker;
    }
    proc_irq_restore(flags);
    return ok;
}
void net_worker_stop(void)
{
    uintptr_t flags = proc_irq_save();
    if (net_worker_running()) {
        stopping = true;
        proc_wakeup(&channel);
        while (!stopped) proc_sleep(&stop_channel, NX_NET_WORKER_PRIORITY);
    }
    proc_irq_restore(flags);
}

bool net_worker_submit(struct nx_net_work *work)
{
    uintptr_t flags = proc_irq_save();
    bool ok = net_worker_running() && !stopping &&
        nx_net_queue_put(&queue, work) != NX_NET_PUT_FULL;
    if (ok) proc_wakeup(&channel);
    proc_irq_restore(flags);
    return ok;
}
bool net_worker_cancel(struct nx_net_work *work)
{
    uintptr_t flags = proc_irq_save();
    bool removed = nx_net_queue_remove(&queue, work);
    proc_irq_restore(flags);
    return removed;
}

struct call {
    struct nx_net_work work;
    void (*function)(void *);
    void *argument;
    volatile bool done;
};
static void call_trampoline(void *argument)
{
    struct call *call = argument;
    call->function(call->argument);
    uintptr_t flags = proc_irq_save();
    call->done = true;
    proc_wakeup(call);
    proc_irq_restore(flags);
}
bool net_worker_call(void (*function)(void *), void *argument)
{
    struct call call = { .function = function, .argument = argument };
    nx_net_work_init(&call.work, call_trampoline, &call);
    uintptr_t flags = proc_irq_save();
    bool ok = net_worker_submit(&call.work);
    while (ok && !call.done) proc_sleep(&call, NX_NET_WORKER_PRIORITY);
    proc_irq_restore(flags);
    return ok;
}

void net_timer_arm(struct nx_net_timer *timer, uint64_t ticks)
{
    require_worker();
    nx_net_timer_arm(&timers, timer, timer_ticks(), ticks);
}
bool net_timer_cancel(struct nx_net_timer *timer)
{
    require_worker();
    return nx_net_timer_cancel(&timers, timer);
}
uint64_t net_ticks_from_ms(uint64_t milliseconds)
{
    uint64_t whole = milliseconds / 1000, part = milliseconds % 1000;
    if (whole > UINT64_MAX / NX_CLK_TCK - 1) return UINT64_MAX;
    return whole * NX_CLK_TCK + (part * NX_CLK_TCK + 999) / 1000;
}
struct net_worker_stats net_worker_stats(void)
{
    uintptr_t flags = proc_irq_save();
    struct net_worker_stats stats = { queue.enqueued, queue.coalesced, queue.overflows, completed,
        timers.armed, timers.cancelled, timers.expired, sleeps };
    proc_irq_restore(flags);
    return stats;
}

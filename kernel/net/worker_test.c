#include "net/worker.h"
#include "callout.h"
#include "console.h"
#include "memory.h"
#include "process.h"
#define REQUIRE(c) do { if (!(c)) panic("network worker self-test: " #c); } while (0)

#define CALLERS 3u
#define CALLS 40u
static unsigned counter, finished_callers;
static bool context_seen;

static void increment(void *argument)
{
    (void)argument;
    context_seen = net_worker_context();
    unsigned value = counter;
    proc_yield();   /* Preemption inside a callback must not break serialization. */
    counter = value + 1;
}
/* A socket caller: sleeps in net_worker_call while the worker progresses. */
static void caller(void *argument)
{
    (void)argument;
    for (unsigned i = 0; i < CALLS; ++i) REQUIRE(net_worker_call(increment, NULL));
    uintptr_t flags = proc_irq_save();
    ++finished_callers;
    proc_wakeup(&finished_callers);
    proc_irq_restore(flags);
    proc_exit(0);
}

/* IRQ producer: the callout runs in timer-interrupt context. */
static struct nx_net_work irq_work;
static volatile unsigned irq_runs;
static volatile bool irq_submitted, irq_coalesced;
static void irq_handler(void *argument)
{
    (void)argument;
    ++irq_runs;
    proc_wakeup((void *)&irq_runs);
}
static void irq_producer(void *argument)
{
    (void)argument;
    irq_submitted = net_worker_submit(&irq_work);
    irq_coalesced = net_worker_submit(&irq_work);   /* still pending: coalesces */
}

/* Overflow: the worker is busy inside fill, so nothing drains meanwhile. */
static struct nx_net_work flood[NX_NET_WORK_LIMIT + 1];
static unsigned flood_runs, flood_accepted;
static bool flood_cancelled;
static void flood_handler(void *argument) { (void)argument; ++flood_runs; }
static void fill(void *argument)
{
    (void)argument;
    for (unsigned i = 0; i <= NX_NET_WORK_LIMIT; ++i) {
        nx_net_work_init(&flood[i], flood_handler, NULL);
        flood_accepted += net_worker_submit(&flood[i]);
    }
    flood_cancelled = net_worker_cancel(&flood[3]) && !net_worker_cancel(&flood[3]);
}

/* Timers: order, cancellation, rearm and periodic rearm from the callback. */
static struct nx_net_timer timers[5];
static uint64_t armed_at, fired_at[5];
static unsigned order[8], fired, periodic;
static void timer_fired(void *argument)
{
    unsigned index = (unsigned)(uintptr_t)argument;
    REQUIRE(net_worker_context());
    fired_at[index] = timer_ticks();
    if (fired < 8) order[fired++] = index;
    if (index == 4 && ++periodic < 3) net_timer_arm(&timers[4], 2);
    if (index == 1) REQUIRE(!net_timer_cancel(&timers[1]));   /* already expired */
}
static void arm_timers(void *argument)
{
    (void)argument;
    for (uintptr_t i = 0; i < 5; ++i) nx_net_timer_init(&timers[i], timer_fired, (void *)i);
    armed_at = timer_ticks();
    net_timer_arm(&timers[0], 20);
    net_timer_arm(&timers[1], 2);
    net_timer_arm(&timers[2], 3);
    net_timer_arm(&timers[3], 1);
    net_timer_arm(&timers[3], 25);         /* rearm moves it later */
    net_timer_arm(&timers[4], 1);
    REQUIRE(net_timer_cancel(&timers[2]) && !net_timer_cancel(&timers[2]));
}

static void tests(void *argument)
{
    (void)argument;
    int32_t status;
    REQUIRE(!net_worker_submit(&irq_work) && !net_worker_call(increment, NULL));
    REQUIRE(net_worker_start() && !net_worker_start() && net_worker_running());
    REQUIRE(!net_worker_context());

    for (unsigned i = 0; i < CALLERS; ++i) REQUIRE(proc_spawn(caller, NULL) > 0);
    uintptr_t flags = proc_irq_save();
    while (finished_callers != CALLERS) proc_sleep(&finished_callers, 30);
    proc_irq_restore(flags);
    REQUIRE(counter == CALLERS * CALLS && context_seen);
    console_puts("NET WORKER CALLS OK\r\n");

    nx_net_work_init(&irq_work, irq_handler, NULL);
    struct net_worker_stats before = net_worker_stats();
    flags = proc_irq_save();
    REQUIRE(callout_schedule(irq_producer, NULL, 1) == 0);
    while (!irq_runs) proc_sleep((void *)&irq_runs, 30);
    proc_irq_restore(flags);
    struct net_worker_stats after = net_worker_stats();
    REQUIRE(irq_submitted && irq_coalesced && irq_runs == 1);
    REQUIRE(after.coalesced == before.coalesced + 1);

    uint64_t overflows = net_worker_stats().overflows;
    REQUIRE(net_worker_call(fill, NULL));
    REQUIRE(flood_accepted == NX_NET_WORK_LIMIT && flood_cancelled);
    REQUIRE(net_worker_stats().overflows == overflows + 1);
    while (flood_runs != NX_NET_WORK_LIMIT - 1) proc_delay(1);
    console_puts("NET WORKER QUEUE OK\r\n");

    /* Worker sleeps between timers; only its deadline can wake it. */
    REQUIRE(net_worker_call(arm_timers, NULL));
    proc_delay(30);
    REQUIRE(fired == 6);
    const unsigned expected[] = {4, 1, 4, 4, 0, 3};
    for (unsigned i = 0; i < 6; ++i) REQUIRE(order[i] == expected[i]);
    REQUIRE(fired_at[1] >= armed_at + 2 && fired_at[0] >= armed_at + 20);
    REQUIRE(fired_at[3] >= armed_at + 25 && !fired_at[2]);
    REQUIRE(net_worker_stats().timers_cancelled == 1);
    console_puts("NET WORKER TIMERS OK\r\n");

    /* Stop cancels armed timers and exits; the starter reaps the worker. */
    REQUIRE(net_worker_call(arm_timers, NULL));
    net_worker_stop();
    REQUIRE(!net_worker_running() && !net_worker_submit(&irq_work));
    REQUIRE(proc_wait(&status) > 0 && status == 0);
    for (unsigned i = 0; i < CALLERS; ++i) REQUIRE(proc_wait(&status) > 0 && status == 0);
    REQUIRE(proc_wait(NULL) < 0);
    REQUIRE(!timers[0].pending && !timers[4].pending);
    REQUIRE(net_ticks_from_ms(0) == 0 && net_ticks_from_ms(1) == 1 &&
            net_ticks_from_ms(200) == 20 && net_ticks_from_ms(500) == 50);
    proc_exit(0);
}

void net_worker_selftest(void)
{
    uint32_t before = page_free_count();
    v7_pid_t pid = proc_spawn(tests, NULL);   /* process_selftest initialized the table */
    REQUIRE(pid > 0);
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == pid && status == 0);
    REQUIRE(page_free_count() == before);
    console_puts("NET WORKER OK\r\n");
}

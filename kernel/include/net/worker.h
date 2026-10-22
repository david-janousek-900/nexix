#ifndef NEXIX_NET_WORKER_H
#define NEXIX_NET_WORKER_H
#include "net/sched.h"

/* Serialized network worker (task 415). One schedulable kernel process runs
 * all protocol work and timers, so network state needs no further locking.
 * Work callbacks and timer callbacks run in that process with IRQs enabled;
 * they must not sleep, because that would stall all network progress.
 */
#define NX_NET_WORK_LIMIT 64u   /* Pending work items; producers drop beyond. */
#define NX_NET_WORK_BATCH 16u   /* Items or timers per pass before yielding. */
#define NX_NET_WORKER_PRIORITY 26u

/* Process context after proc_init. The calling process is the worker's parent
 * and reaps it after net_worker_stop. False if already running or no slot.
 */
bool net_worker_start(void);
/* Make the worker a child of the bootstrap process instead of the caller, so
 * no user process waits for it (a stack started on demand, task 425). */
void net_worker_detach(void);
/* Process context. Runs queued work, cancels armed timers, waits for exit. */
void net_worker_stop(void);
bool net_worker_running(void);
/* True only inside work and timer callbacks. */
bool net_worker_context(void);

/* Any context, including IRQ handlers: never sleeps. Queues the item, or
 * coalesces with its pending copy, and wakes the worker. False when the queue
 * is full or the worker is not running; the producer then drops its input.
 */
bool net_worker_submit(struct nx_net_work *);
bool net_worker_cancel(struct nx_net_work *);
/* Process context: run function(argument) on the worker, sleeping until it
 * completes. False when the queue is full or the worker is not running.
 */
bool net_worker_call(void (*function)(void *), void *argument);

/* Worker context only (panics otherwise). Delays are in clock ticks. */
void net_timer_arm(struct nx_net_timer *, uint64_t ticks);
bool net_timer_cancel(struct nx_net_timer *);
uint64_t net_ticks_from_ms(uint64_t milliseconds);   /* Rounds up. */

struct net_worker_stats {
    uint64_t enqueued, coalesced, overflows, completed;
    uint64_t timers_armed, timers_cancelled, timers_expired, sleeps;
};
struct net_worker_stats net_worker_stats(void);

void net_worker_selftest(void);
#endif

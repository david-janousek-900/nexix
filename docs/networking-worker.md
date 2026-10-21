# Serialized network worker and protocol timers (task 415)

`kernel/include/net/sched.h` and `kernel/net/sched.c` provide an allocation-free
work queue and protocol timer list. `net/worker.h` and `kernel/net/worker.c` run
them on one schedulable kernel process. These are fresh native implementations
under Nexix's root BSD-3-Clause license. Nothing starts the worker during normal
boot yet. Task 418 starts it when interfaces attach.

## Replacing BSD interrupt levels

4.4BSD serializes network state with `splnet`/`splimp`. Its NIC interrupts queue
packets with `schednetisr`, and protocol work runs at software-interrupt level.
Nexix replaces all of this with **one worker process**:

| BSD mechanism | Nexix adaptation |
|---|---|
| `splnet`/`splimp` critical sections | All protocol state is touched only by the worker; no network locks |
| `schednetisr`, `ipintrq`/`IF_ENQUEUE` with `ifq_maxlen` | `net_worker_submit` of an intrusive work item into a bounded queue |
| `pr_fasttimo` (200 ms), `pr_slowtimo` (500 ms) and `TCPT_*` countdowns | Individual one-shot timers at absolute 64-bit tick deadlines |
| `tsleep`/`wakeup` in socket calls | `net_worker_call`: queue a request and sleep until the worker completes it |

Work and timer callbacks run in the worker with IRQs enabled. The scheduler may
preempt them, but no other network code runs meanwhile. Callbacks **must not
sleep**, because a sleeping callback would stall all network progress.
`net_worker_context()` reports whether the caller is inside such a callback.
Timer operations panic when used outside one.

## Work queue

- Items are embedded in their owners (`struct nx_net_work`), so submitting needs
  no allocation.
- `net_worker_submit` can be called from any context, including IRQ handlers. It
  disables IRQs, queues the item, calls `proc_wakeup` on the worker and returns.
  It never sleeps.
- Submitting an item that is still pending **coalesces** with it, as repeated
  `schednetisr` calls do.
- At most `NX_NET_WORK_LIMIT` (64) items can be pending. A full queue refuses the
  item, counts an overflow and returns false. The producer then drops its input,
  just as BSD drops on `IF_QFULL`.
- `net_worker_cancel` removes a pending item. Once it is taken off the queue, an
  item may be submitted again, even from its own callback.
- On each pass the worker runs at most 16 work items and 16 due timers in FIFO
  order, then yields so that user processes keep running under load.

## Timers

`struct nx_net_timer` is a one-shot timer, sorted by deadline and FIFO among equal
deadlines.

- `net_timer_arm` sets the deadline to now + ticks, saturating at the maximum.
  Re-arming a pending timer moves it.
- A successful `net_timer_cancel` guarantees the callback will not run for that
  arming. Cancellation is exact because only the worker touches timers.
- Expiry removes the timer before calling it, so a callback may re-arm itself (for
  periodic protocol timers) or cancel others.
- `net_ticks_from_ms` converts milliseconds to 100 Hz ticks, rounding up.
  Protocols arm per-connection timers directly instead of counting down in
  `slowtimo`.

When it has nothing to do, the worker sleeps on its channel with the scheduler's
timed-sleep deadline set to the earliest timer. A submit or that deadline wakes
it. There is no extra hook in the timer interrupt and no polling.

## Life cycle

`net_worker_start` spawns the worker from process context after `proc_init`. The
calling process becomes its parent. `net_worker_stop` stops new submissions,
lets the worker drain the queued work, cancels any armed timers, waits for the
worker to exit and leaves the parent to reap it. `net_worker_stats` reports
counts of enqueued, coalesced, overflowing and completed work, of armed,
cancelled and expired timers, and of worker sleeps.

## Validation

`make test-net-sched` runs host fixtures under ASan/UBSan. They cover:

- FIFO order and coalescing;
- the exact overflow limit, including a zero limit;
- removal from the middle and tail, and refusal of an item on another queue;
- requeueing an item after it is taken;
- deadline order, FIFO among equal deadlines, re-arm moves, saturation and
  bounded expiry passes;
- callbacks that re-arm themselves and cancel later due timers.

`make test-net-worker-guest` (the same self-test boots as `test-net-pools-guest`)
checks the following at 16 MiB and 128 MiB:

- Three processes each make 40 sleeping `net_worker_call` requests. A callback
  yields in the middle of a read-modify-write, and no updates are lost.
- A timer-interrupt callout submits an item twice, and it coalesces.
- A busy worker floods the queue to its exact limit; the overflow is counted and
  a cancelled item does not run.
- Timers expire in deadline order with no early expiry. A cancelled timer never
  fires, and a periodic timer re-arms itself from its callback. The test caller
  only sleeps, so only the worker's deadline can wake it.
- Stopping cancels armed timers, refuses later submits and lets the parent reap
  the worker, with no pages leaked.

The required boot markers are `NET WORKER CALLS OK`, `NET WORKER QUEUE OK`,
`NET WORKER TIMERS OK` and `NET WORKER OK`.

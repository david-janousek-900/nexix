# Scheduling and kernel timing

## V7 scheduler (task 362)

The resident scheduler retains round-robin selection among equal priorities.
Every running-process tick increments an 8-bit saturating CPU-usage counter.
Once per second, live processes age up to 127 seconds and usage decays using
V7's `cpu * 8 / 10 + nice - 20`, clamped to 0–255. User priority is
`50 + cpu / 16 + nice - 20`, clamped to 0–127. Nice values remain 0–39;
only root can decrease nice. A newly forked process starts with zero usage.

Sleep retains the requested kernel wakeup priority. User priority is recalculated
when the sleeping syscall resumes and on return to user mode. The existing
five-tick preemption quantum remains; swapping is outside the resident model.

`make test-scheduler-unit` checks saturation, decay, nice extremes, kernel sleep
priorities, process aging and selection order. `make test-process` exercises
actual PIT preemption, sleepers, wakeups, fork and wait on the native kernel.

## Callout queue (task 364)

`callout_schedule(function, argument, delay)` schedules a non-sleeping kernel
callback after a monotonic tick delay. The 20-entry queue retains V7's bounded
capacity and FIFO order for equal deadlines. Zero delay means the next tick.
Insertion, cancellation and dispatch serialize with interrupts disabled.
Invalid callbacks/delays return EINVAL; a full queue returns ENOSPC rather than
panicking the system. Delays may not exceed INT64_MAX ticks.

`callout_cancel` removes all queued instances matching a function and argument;
it cannot undo a callback already executing. PIT dispatch removes an entry before
calling it, allowing callbacks to cancel or schedule entries safely. Dispatch is
non-reentrant and bounded to 20 callbacks per tick. Signed deadline comparisons
handle tick wrap within the supported delay horizon. Callbacks must not block,
yield or perform unbounded work. Existing device-specific timers retain their
current interfaces; their future callout consumers can use this API.

`make test-callout-unit` covers ordering, deadlines, queue exhaustion,
cancellation, callback rearming and wraparound. `make test-process` also schedules
a real PIT callback that wakes a sleeping kernel process (`CALLOUT IRQ OK`).

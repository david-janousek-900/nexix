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

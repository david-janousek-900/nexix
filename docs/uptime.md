# Uptime and scheduler load

`uptime` is a native Nexix extension. It prints local time, elapsed hours/minutes
(and days after 24 hours), the number of occupied utmp sessions, and 1/5/15-minute
load averages. Elapsed time comes from kernel boot ticks, independently of RTC
or later date changes. Ordinary users can run it.

`NX_KINFO_LOAD` extends the versioned snapshot interface with three fixed-point
averages, their scale, runnable count and process count. It exposes no private
addresses and is public like process snapshots; other table kinds stay root-only.
The scheduler samples once per second. Decays are rounded `65536*exp(-1/tau)` for
60, 300 and 900 seconds; 64-bit intermediate arithmetic avoids kernel FPU use.
Samples count runnable user processes and uninterruptible waits, excluding
system processes, stopped/zombie processes and interruptible/timed sleeps.
Averages start at zero and are not normalized by CPU count.

`make test-uptime` checks deterministic load growth, decay and convergence,
serializer bounds/permissions, actual CPU workloads, wall-clock changes,
login/logout accounting and ordinary-user output at 16/128 MiB.

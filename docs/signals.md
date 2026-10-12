# Signals and init/shell services (task 15)

Nexix adapts the retained V7 `sig.c` and `sys4.c` behavior for native ELF64
processes. Signal numbers 1–15 retain their V7 meanings; the pending bitmap
also supports V7's unused slot 16. Default actions terminate the process with
the signal number in the low wait-status bits. Core dumps, tracing, signal
masks, restartable syscalls, and terminal job control remain outside this task.

## Delivery and return

`signal(n, handler)` returns the previous disposition. SIG_DFL is zero and
SIG_IGN is one. Invalid numbers and changing SIGKILL return EINVAL. Caught
handlers reset to default before entry, except SIGILL and SIGTRAP, matching
V7. Installing a disposition clears that signal's pending bit. Pending signals
coalesce by number and are processed in ascending order on return to user mode.
Ignored signals neither interrupt a sleep nor terminate the process.

Delivery runs on syscall, user fault and IRQ return. Handler code and its libc
restorer must have executable user mappings. The kernel reserves the SysV red
zone, writes a 688-byte `nx_signal_context` (176 integer bytes followed by
512 bytes of FXSAVE64 state) to writable user stack memory and
pushes the restorer address. Handler entry has RSP modulo 16 equal to eight,
RDI containing the signal number, and DF/TF clear. The context immediately
follows the return address and contains all general registers and the original
RIP, RSP, selectors and flags. It is defined in `include/nexix/signal.h`.

The libc restorer passes the saved context address to native syscall 70,
sigreturn. This restores the interrupted result register as well as the other
registers. The kernel checks user selectors, allowed RFLAGS with IF enabled
and IOPL zero, executable RIP and writable stack before installing the frame.
An invalid context returns EFAULT or EINVAL without replacing the live frame.
User contexts can change their own registers; they cannot acquire kernel
privilege. No kernel chain survives a handler, so native setjmp/longjmp can
leave a handler without leaking signal state. These routines save SysV
callee-saved registers, stack, return address, x87 control and MXCSR; they do not
save signal masks. FP contexts and their validation are described in
[floating-point.md](floating-point.md).
Handlers which use errno must preserve it themselves if needed.

An unusable delivery stack terminates the process with SIGSEGV. Ordinary CPU
faults in a managed user process produce SIGILL, SIGFPE, SIGTRAP or SIGSEGV.
An ignored synchronous fault terminates rather than repeatedly returning to
the failing instruction. Kernel faults, double faults, NMIs and the deliberately
unmanaged bootstrap protection tests retain their fatal diagnostic behavior.

## Blocking calls, alarms and groups

Pipe and terminal I/O, wait and pause use interruptible sleep. They check for
pending signals before publishing the sleeping state and after waking, with
IRQs disabled throughout that transition. A caught signal returns EINTR if
nothing transferred, or the completed byte count after a partial write. Wait
leaves live children available for later collection. Writes without pipe
readers post SIGPIPE and return EPIPE if the signal is ignored or caught.

`alarm(seconds)` sets a PIT deadline and returns the previous remaining time,
rounded up to seconds. Zero cancels it. Alarm deadlines survive exec, but fork
starts the child without pending signals or an alarm. Fork inherits handlers
and restorers. Exec resets caught handlers and preserves ignored dispositions.

Kill accepts a positive PID, zero for the caller's group, a negative group
number, and -1 for all eligible user processes except PID 1 and the caller.
Signal zero checks existence and permission without posting a signal. Root
can signal any eligible process; other callers must match its effective UID.
Missing targets return ESRCH; denied matching targets return EPERM. Kernel
helper processes and unpublished/zombie slots are excluded.

`setpgrp()` creates the caller's group using its PID and associates its
controlling terminal with that group; `getpgrp()` reports it. Fork inherits the
group. This fills V7's reserved call 39 with a native no-argument interface.
Cooked and cbreak Ctrl-C/Ctrl-backslash flush input, release output stop, and
post SIGINT/SIGQUIT to the terminal group. RAW treats these bytes as data.
There are no foreground/background access restrictions or sessions yet.

## Heap, credentials, clock and metadata

Brk starts at the page-aligned end of the highest ELF load segment. Native
syscall 17 with address zero queries the current break; otherwise it sets the
break and returns zero. Libc brk/sbrk implement the normal pointer interface.
Growth allocates zeroed RW/NX pages, rolls back completely on allocation failure,
and preserves the old break and data. Shrinking releases whole pages while
keeping a partial final page. Image pages and the guarded fixed stack cannot
be removed or overwritten. Fork copies the break and heap; exec installs a
fresh heap. Libc heap allocation is implemented; demand paging and automatic stack growth
remain outside the current memory model.

Setuid/setgid follow V7: root, or a caller selecting its existing real ID, sets
both real and effective IDs. Other changes return EPERM; wide raw arguments
are rejected instead of truncating. Nice adjusts the inherited niceness within
0–39 around baseline 20 and updates runnable priority; only root can decrease
it. CPU-usage priority decay remains pending.

Time/stime use signed 64-bit seconds. The wall clock starts from the UTC RTC at boot
and advances with PIT ticks; root can set it through stime. An invalid RTC
falls back to epoch zero plus uptime with a boot diagnostic. Calendar/timezone
libc functions and configurable TZ are implemented in task 24; see [time.md](time.md). Alarms use monotonic PIT ticks independently of
stime. Access uses real IDs, including directory search checks; open uses
effective IDs. Chmod requires ownership or root; chown requires root. Chmod
preserves the file type and restricts the sticky bit to root. Utimes are passed
as two native 64-bit seconds and must fit the disk's unsigned 32-bit fields.
Metadata updates reject read-only mounts and preserve inode block ownership.
Automatic access/modification/change timestamps are maintained by task 23.
Utime sets the requested atime/mtime and stamps ctime from the system clock;
chmod and chown update ctime without changing atime/mtime.
Sync is a successful no-op for the synchronous write-through cache, reporting
EIO if its mounted filesystem has failed. These services have libc wrappers.

## Validation

```sh
make TOOLCHAIN_PREFIX= test-signals  # QEMU, 16 and 128 MiB
make -j3 TOOLCHAIN_PREFIX= test      # full regression suite
```

Normal boot also runs the native ring-3 fixture. It checks ignored/caught/default
signals, nested handlers, one-shot and persistent dispositions, complete
register and CF/DF restoration, nonlocal handler exit, invalid signal returns,
alarm cancellation/expiry/inheritance, exec rules, fault termination, group
signaling, interrupted pipe read/write, terminal read and wait, caught/default
SIGPIPE, heap zeroing/fork/shrink/OOM rollback, real/effective ID checks, metadata
updates on writable/read-only mounts and full process/page/file cleanup. Host
TTY tests verify signal controls flush input and release stopped output, and
that RAW preserves those bytes. Test-tty additionally sends Ctrl-C and
Ctrl-backslash through QEMU COM1 to a sleeping child and checks both parent
and child receive their terminal group signal.

Success markers are NEXIX_SIGNALS_OK, NEXIX_HEAP_OK, NEXIX_INIT_SYSCALLS_OK and
NEXIX_SIGNALS_CLEANUP_OK, followed by NEXIX_BOOT_OK. Logs are
`build/signals-16M.log` and `build/signals-128M.log`.

Task 16 rebuilds V7 init, the shell and basic utilities. The shell and utilities
run in acceptance tests, and task 18 implements normal init/shell startup.
Multi-user init/getty/login integration remains tasks 98–108.


Task 52 adds [native core dumps](core-dumps.md) for V7's default core-producing
signals and fatal signal-frame failures. Successful output sets wait bit 0200;
failed/ineligible output leaves it clear. Caught/ignored signals retain their
existing behavior. Core policy requires equal real/effective UID and GID.

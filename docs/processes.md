# Native process scheduling and lifecycle (step 8)

`kernel/v7/process.c` adapts the process behavior in the original V7
`usr/sys/sys/slp.c` and `sys1.c`. The implementation preserves wait-channel
sleep/wakeup, runnable/sleeping/zombie states, priority selection, forked process
metadata, encoded exit status, blocking wait, and orphan reparenting. It is a
native implementation based on those sources, not an unchanged build of the
PDP-11 scheduler. Original sources and license remain under `reference/`.

## Process ownership

The table retains V7's 150 slots. Slot zero represents the bootstrap scheduler
and fallback reaper; up to 149 other slots hold processes. A builder reserves an
unpublished SIDL slot, a private address space, and a kernel stack. Process
builders and direct table access require interrupts disabled. Failed builds
abort without publishing the process. PIDs wrap below 30000 and avoid active
PIDs and process-group IDs. Non-root owners are limited to 25 processes and
cannot consume the final slot.

Stacks come from a fixed BSS pool, with 16 KiB and a lower unmapped guard page
per slot. The pool is reserved from the physical allocator at boot (about 3 MiB),
not dynamically allocated per process. A slot exclusively owns its stack while
live, and its stack is cleared when reused. User areas also come from a fixed
per-slot pool. TSS.RSP0 changes with the active process. Existing double-fault
and NMI IST stacks remain independent of these stacks.

Each private user mapping owns a distinct allocator frame. The process builder
allocates, clears or copies, and maps these frames; it permits no aliases or
shared leaves. The process VM owns its page tables. The shared supervisor kernel
mapping belongs to the platform, not to a process. The page-table walker skips
that mapping and enumerates private lower-canonical user leaves.

## Switching and scheduling

`arch/x86_64/context.S` preserves RBX, RBP, R12–R15, RSP, and the continuation RIP
required by the kernel C calling convention. Interrupt entry already saves all
user general registers. A user process starts or resumes through the common
interrupt-return path, using its own complete trap frame.

The scheduler runs on the bootstrap stack. Interrupts remain disabled while
selecting a runnable process, changing the current-process pointer, activating
CR3, updating TSS.RSP0, and switching stacks. Selection favors the lowest numeric
priority, with round robin selection among equal priorities. A waking task keeps
its sleep priority until `proc_sleep` resumes, then restores its previous
priority. This prevents a completed wait from permanently outranking new tasks.
V7's CPU-usage priority decay, swapping, and full signal behavior are deferred.

The 100 Hz PIT preempts a running kernel or user process after five ticks, once
PIC EOI has been sent. The outgoing IRQ handler remains suspended on that
process's kernel stack. On resumption, it returns through IRET, restoring the
original IF and user registers. Preemption never switches out code protected
by CLI. Accounting stores user/system ticks at the actual PIT rate; u_start is
currently uptime in seconds, not a real-time clock value.

`proc_sleep` sets the channel and sleeping state before switching out;
`proc_wakeup` makes every matching sleeper runnable. Callers must disable IRQs
across checking a shared condition and entering sleep, and must recheck the
condition after wakeup. Interruptibility by signals is not yet implemented.
Deadline sleeps permit timer wakeup while the scheduler has no runnable task.
The idle path uses an atomic STI/HLT/CLI sequence to avoid missing that wakeup.
The bootstrap scheduler returns when no published non-zombie task remains.

## Fork, exit, and wait

`proc_fork` takes a live CPL3 trap frame. It clones every private user leaf
eagerly into a new physical frame, including RX code and read-only data, and
preserves mapping permissions. It copies per-process metadata, rebases pointers
into the child user area, resets child accounting, and installs a child trap
frame with RAX zero. The parent receives the child PID. Failed allocations
release all copied data frames and page tables before returning a negative V7
errno; the parent mappings remain untouched. Kernel C continuations are not
forked. This native return convention is for the upcoming ABI, not PDP-11
binary compatibility.

Until filesystem ownership hooks are implemented, fork rejects processes with
file, directory-inode, or shared-text references with EBUSY. Current test and
builder processes hold none. Descriptors, inode lifetime, signals, executable
loading, FPU state, and user TLS require later implementation. Fatal user faults
still panic the kernel rather than terminating just the offending process.

Exit saves status and cumulative tick accounting, marks the process a zombie,
wakes its parent, and reparents its children to live PID 1 (or bootstrap PID 0
when PID 1 is absent or exiting). It cannot free its active stack or address
space. After switching to the scheduler stack and kernel CR3, the scheduler
releases its data frames and page tables. Wait then collects the zombie's PID,
encoded status and accounting, and recycles its user/stack slot. A parent with
live children sleeps atomically on its own process entry; a parent without
children receives ECHILD. Bootstrap collects its children without sleeping.
Stopped-child reporting and signal-related statuses are deferred.

## Validation

Normal boot runs kernel scheduling tests followed by a ring 3 fork/exit/wait
test through a deliberately limited test interrupt gate. Its commands 0x100,
0x101, 0x102 and 0x104 exercise fork, wait, exit and tick accounting only while
the process test is active. This gate is not a supported syscall interface.
The tests finish by reclaiming every process and checking the physical free
count against its starting value.

Run:

```sh
make TOOLCHAIN_PREFIX= smoke          # 128 MiB normal boot
make TOOLCHAIN_PREFIX= test-process   # same checks with 16 MiB RAM
make -j3 TOOLCHAIN_PREFIX= test        # complete regression suite
```

Successful serial output includes `NEXIX_PROCESS_SWITCH_OK`,
`NEXIX_PROCESS_PREEMPT_OK`, `NEXIX_PROCESS_SLEEP_OK`, `NEXIX_PROCESS_WAIT_OK`,
`NEXIX_PROCESS_SLOTS_OK`, `NEXIX_PROCESS_OOM_OK`, `NEXIX_PROCESS_FORK_OK`, and
`NEXIX_PROCESS_CLEANUP_OK`, followed by `NEXIX_BOOT_OK`. Detailed assertions and
failure cases are listed in [validation.md](validation.md).

Step 9 establishes syscall entry, numbering, return/error conventions, and safe
access to user memory; step 10 adds ELF64 loading and exec.

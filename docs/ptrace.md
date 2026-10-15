# V7 tracing and native process access (task 57)

The sources are V7 `sys/sig.c` (`issig`, `stop`, `psig`, `ptrace`,
`procxmt`), `sys/sys1.c` (exec/exit/wait), `trap.c`, `sysent.c`,
`h/{proc,user,reg}.h`, libc `sys/ptrace.s` and `ptrace(2)`.
The reference remains unchanged. Nexix uses resident x86_64 mappings and saved
interrupt/FPU contexts instead of PDP-11 segmentation and a global IPC mailbox.

Include `<sys/ptrace.h>` (also included by historical `<signal.h>` callers):
`long ptrace(int request, pid_t pid, void *address, long data)`.
Words are aligned eight-byte native longs. A successful read can return any
signed bit pattern, including -1; libc clears errno before every call and sets
it only on error. Non-read requests return zero. The raw syscall is number 26:
request, PID, address, data, writable uint64 result pointer. It returns zero or
negative errno; the fifth argument receives requests 1–3 results. This output
slot avoids collision between negative words and the native syscall error ABI.
Other requests ignore it.

| Request | Behavior |
|---|---|
| <=0 | Mark caller traced; other arguments ignored. Fork does not inherit tracing. |
| 1 / 2 | Read an aligned word from the stopped child's user mappings; native instruction/data spaces coincide. |
| 3 | Read an aligned word at a byte offset in `struct nx_ptrace_user`. |
| 6 | Write a permitted register/FPU word at the same snapshot offset. |
| 7 | Continue, replacing the pending signal set with data (0 suppresses it); address 1 keeps PC, otherwise selects a mapped executable user address. |
| 8 | Terminate the stopped child with its first pending signal as wait status, without a core dump. |

Positive requests require a direct child in SSTOP. No arbitrary attach or root
exception exists. Running/sleeping children, siblings, unrelated processes and
reaped PIDs return ESRCH. Invalid requests, unaligned/unmapped child addresses,
invalid register offsets and invalid continue signals return EIO. An invalid
caller output slot returns EFAULT. Requests 4, 5 and 9 remain EIO until task 58.
Remote memory writes, single-step and a rebuilt debugger remain separate tasks.

Any pending signal, including one whose disposition is ignored, stops a traced
process before delivery. A successful traced exec suppresses set-ID transitions
and stops with SIGTRAP before the first instruction in the new image. Wait
returns `(signal << 8) | 0177` once per stop, without reaping the child or adding
its CPU time to the parent's child totals. `WIFSTOPPED` and `WSTOPSIG` decode it.
Like V7, an examination request clears SWTED so a later wait can report the
still-stopped child again. A subsequent wait otherwise blocks and remains
interruptible. Continue suppresses, replaces or delivers the selected signal
once; subsequent signals stop again. Ignored replacements are discarded.
SIGKILL also follows V7 tracing stops; request 8 can terminate a stopped child.

Stop wakes the waiting parent and switches to the scheduler, which does not
run SSTOP children. The parent examines the child directly with IRQs disabled;
access requests keep it stopped. Scheduler switching saves/restores the FPU.
Parent exit reparents children and resumes stopped ones; an orphaned trace
exits with its pending signal rather than remaining stopped forever. V7's
special rule for a child of init is retained: it exits instead of trace-stopping.

The public snapshot has a 176-byte register prefix and 512-byte FXSAVE64 state
(total 688 bytes), with no private user-area fields or kernel pointers. Use
`NX_PT_OFFSET(field)` rather than historical user-area offsets. General registers,
RIP, RSP, RFLAGS and FPU words are writable; selectors and trap vector/error are
read-only. RIP must name executable user memory and RSP must point above writable
user memory. RFLAGS forces user interrupt enable and strips privileged/reserved
bits and TF (stepping is task 58). MXCSR validation rejects unsupported bits
before changing the saved FPU state. Failed validation leaves registers intact.

Run `make test-ptrace`. Disposable-root QEMU acceptance at 16/128 MiB checks
parent-only access, running-child rejection, ignored-signal stops, wait-once and
EINTR, negative read words/errno, memory and snapshot bounds, invalid caller
pointers, general/FPU edits that survive resumption, flag/selector protection,
invalid MXCSR rollback, repeated stops, signal suppression/replacement (including
caught/ignored replacements and V7 SIGKILL stops), PC redirection, forced exit,
synchronous faults, traced exec and set-ID suppression.
A disposable traced grandchild verifies debugger-exit orphan release; the
bootstrap checks its signal status and reaps it. Fork tracing non-inheritance
is also checked. The root-exec harness checks process/file/TTY/page and filesystem
reclamation.
Header layout/export and inventory audits run through the same target. No
persistent working disk is used by these tests.

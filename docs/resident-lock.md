# Resident-memory lock (task 63)

Nexix implements V7 `lock(flag)` as syscall 53. Include `<unistd.h>`:

```
if (lock(1) < 0) perror("lock");
/* ... */
if (lock(0) < 0) perror("unlock");
```

The reference implementation is `usr/sys/sys/acct.c:syslock`, registered by
`usr/sys/sys/sysent.c`; `usr/src/libc/sys/lock.s` supplies the C wrapper and
`usr/man/man2/lock.2` documents the call. `usr/sys/h/proc.h` defines SULOCK,
`usr/sys/sys/slp.c` excludes it from ordinary swap-out and initializes forked
children without it, and `usr/sys/sys/sys1.c` clears it on exit. Reference files
remain unchanged.

Effective UID zero is required for both locking and unlocking. Zero clears the
calling process's V7_SULOCK bit; every nonzero int, including negative values,
sets it. Repeated lock/unlock requests succeed. Other process flags are preserved.
Success returns zero; failure returns -1 with EPERM and leaves the lock state
unchanged. The effective-ID check uses the existing V7 suser helper and marks
ASU on successful privilege checks, preserving other accounting flags. A real
UID of zero alone does not authorize the call. Success leaves errno unspecified.
The native raw syscall treats the full argument register as a zero/nonzero value;
it does not truncate high bits. No pointer argument or lock target PID is used.

Fork does not inherit SULOCK. Successful and failed exec preserve it; credential
changes also preserve it. A process that drops root while locked cannot unlock
until it regains effective-root privilege. Exit clears SULOCK before publishing
the zombie. `nx_kinfo` process snapshots expose it as NX_KPROC_USERLOCK, so native
diagnostics can inspect the state without private kernel offsets.

All current Nexix user mappings remain resident, with eager fork and no disk
swapping. Consequently lock/unlock changes the user-controlled state and its
lifecycle, without allocating or pinning extra pages. Heap growth retains the
flag and follows the existing bounded/atomic brk rules; it can still fail on
invalid ranges or insufficient memory. Unlocking does not evict pages. V7's
historical growth-related swapping and compaction-deadlock warning do not require
introducing a swapper or a physical compactor into the native port.

`kernel/v7/lock.c` serializes permission and flag changes with IRQ save/restore.
The syscall dispatch and `libc/lock.c` wrapper use the normal negative-errno ABI.
The existing fork/exec/exit paths already provide the required lifecycle rules.

`make test-lock` runs host permission/flag tests, QEMU root-filesystem acceptance
at 16 and 128 MiB, and inventory, header and export checks. Host tests link the
production suser helper, exercise all 256 initial flag combinations, zero/nonzero
and high-bit values, effective versus real IDs, ASU preservation, denied requests
and IRQ restoration. Native tests check libc/raw calls, flag snapshots, resident
heap growth/invalid-growth rollback, fork isolation, exit/zombie cleanup, failed
and successful exec retention, and denied lock/unlock after dropping privilege
with the lock both set and clear. The exec test reuses the private trace-image
installer and a pipe handshake; it adds no production test syscall.

`make test-unit` includes the host lock fixture. `make test-shell` checks RAM and
IDE shells at both memory sizes.

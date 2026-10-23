# Kernel process accounting (task 56)

The V7 implementation is sys/acct.c (sysacct, acct, compress), h/acct.h,
sys/sys1.c (fork/exec/exit), sys/clock.c (CPU ticks), sys/fio.c (suser),
h/user.h and conf/c.c (accounting state), with rdwri.c for writes. The native
implementation preserves those responsibilities without modifying reference
sources. The syslock function in the same reference acct.c remains task 63.

## Control and inode ownership

`acct(path)` is V7 syscall 51, exported by Nexix libc in `<unistd.h>` and
`<sys/acct.h>`. Effective UID 0 is required; successful privilege checks set
ASU even if a later part of the call fails. The file must already exist and
be regular. Missing files return ENOENT, nonregular files EACCES, invalid
pointers EFAULT, and exhausted managed inode slots ENFILE. Calling with a
non-null path while enabled returns EBUSY before looking up that path.

One global held inode selects the accounting destination; no user descriptor
or open-file-description slot is consumed. Like V7, selection does not test
permission bits, read-only state or executable text ownership. Exit writes
use that held inode without checking the exiting process's file permissions.
They can therefore record unprivileged processes and keep writing after the
pathname is unlinked. The inode remains busy for unmount/root replacement.

`acct(NULL)` disables accounting and releases the held inode; disabling an
already disabled facility succeeds. If final inode release reports an error,
the call reports it but accounting is already disabled. An unprivileged call
returns EPERM and leaves the global state unchanged.

## Lifecycle and fields

Fork resets the child's accounting flags to AFORK (01), resets its own and
child CPU counters and sets its start time. It does not inherit the parent's
ASU flag. Successful exec clears AFORK and changes the command name; failed
exec preserves both. The V7-style suser helper sets ASU (02) on successful
privilege checks: acct, mknod, chroot, privileged ownership/directory operations,
stime, privileged ID changes and negative nice. Nexix's root-only mount/umount
checks also use it. Ordinary UID-0 file permission bypasses and kill's direct
UID check do not set ASU, matching the distinction in the reference code.

Exit closes descriptors, releases cwd/root and executable-text references, then
writes an accounting record before becoming a zombie. The controlling-terminal
device number is saved before cleanup clears it. Signal exits are recorded too.
Records contain the first ten command-name bytes, this process's own user/system
CPU times, elapsed wall time, start time, real UID/GID, controlling TTY and flags.
Children's accumulated CPU times are excluded. ac_mem and ac_io remain zero, as
in the supplied V7 acct.c. There is no added PID or exit-status field.

CPU values retain the native times(2) unit, NX_CLK_TCK/HZ = 100 ticks per second;
elapsed time is in seconds. Both use V7's 13-bit fraction and 3-bit base-8
exponent compression, including rounding. Native 64-bit values beyond the
representable maximum saturate at 0xffff; negative counters and elapsed time
after a backwards clock adjustment clamp to zero. These bounds adapt the
original 32-bit calculation to native counter widths. sa's historical hardcoded
60-Hz arithmetic must be adapted in task 241.

## Native record contract

Records are a headerless sequence of 40-byte little-endian structures. This
uses native 64-bit time_t, consistent with other Nexix user interfaces, and is
not the PDP-11 32-byte accounting-file encoding. Use a fresh native accounting
file; do not mix the two formats. `<sys/acct.h>` supplies `struct acct`/`comp_t`
and flags; kernel globals/pointers from the reference header are private.

| Offset | Field | Size |
|---|---|---|
| 0 | ac_comm, fixed ten bytes; not necessarily NUL terminated | 10 |
| 10, 12, 14 | ac_utime, ac_stime, ac_etime (comp_t) | 2 each |
| 16 | ac_btime, signed epoch seconds | 8 |
| 24, 26 | ac_uid, ac_gid, real identities | 2 each |
| 28, 30, 32 | ac_mem (zero), ac_io (zero), ac_tty | 2 each |
| 34 | ac_flag | 1 |
| 35 | ac_reserved, always zero | 5 |

## Append and storage failures

The single-CPU kernel serializes control and exit writes with interrupts
disabled. Each exit appends at the inode's current EOF; existing contents and
other descriptor offsets are preserved. A bounded filesystem record append
publishes the new EOF only after the complete record is written. Thus a record
crossing a sector boundary cannot expose its first fragment after ENOSPC or a
failed write. Allocations made before a failure may remain owned by the inode
and can be reused or reclaimed by ordinary truncation/unlink. This is not a
journal, power-loss transaction or guarantee against hardware partial sectors.

Write errors do not change the process's exit/wait status or disable accounting,
as in V7. Read-only storage rejects writes; native executable-text exclusion also
evicts unused sticky text and rejects writes while another process owns active
text (an explicit adaptation
to task 51's protection). ENOSPC leaves the visible old length. An I/O failure
retains the existing filesystem fail-stop policy; no production recovery API,
automatic low-space suspension or extended accounting fields are introduced.

## Validation and remaining consumers

`make test-accounting` runs host record/compression/control tests, real filesystem
append fault tests, native QEMU tests at 16/128 MiB, public-header ABI checks,
source/roadmap audits and libc export checks. Native tests cover normal/signal
exit, failed/successful exec, fork flags, privilege tracking, real IDs after
SUID exec, TTY preservation, full descriptor tables, unlinked logs, mount busy
references, read-only volumes, active-text exclusion/sticky eviction, ENOSPC and injected I/O
failure without changing wait status. Host tests check literal golden bytes,
compression boundaries/overflow/clock rollback, cross-sector EOF publication,
data/inode-sector failures and remount persistence. Test gates seed counters
and repair only injected failures in disposable fixtures. Production storage
is preserved; all native tests check final inode/block/page/handle cleanup.

No accounting file or command is installed by this task. accton (task 240),
sa (task 241), login/utmp/wtmp integration and their end-to-end acceptance remain
separate roadmap work. Accounting is disabled at boot until privileged code
selects an existing file.

## Installed accton command (task 240)

`/usr/sbin/accton [file]` selects an existing regular file; no operand disables
accounting. As in V7, only the first operand is used. The active source adapts
`reference/v7/usr/src/cmd/accton.c` with native declarations and checks the
syscall return rather than a possibly stale errno. Success returns 0 and syscall
errors print `accton: <V7 error>` to stderr and return 1. The command never
creates, truncates or changes ownership of the destination. Root privileges
are enforced by the kernel; this executable is not set-ID.

`make test-accton` boots installed disposable roots at 16 and 128 MiB. It checks
missing/nonregular/busy destinations, repeated disabling, ordinary-user denial,
first-operand handling and real cross-user accounting: a uid-101 sum process
must appear in complete 40-byte records with zero padding. After disabling,
further processes leave the record file unchanged. Task 241 still owns sa.

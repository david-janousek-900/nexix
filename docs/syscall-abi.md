# Nexix x86_64 syscall ABI (task 9)

The native ABI is defined by `include/nexix/syscall.h` and
`include/nexix/errno.h`. It is version 2 of the experimental Nexix ABI, for
rebuilt ELF64 programs. It does not implement PDP-11 binary or Linux syscall
compatibility. Call numbers retain V7's `usr/sys/sys/sysent.c` assignments where
applicable; register conventions and pointer-bearing interfaces are native.

## Entry and return

User code executes `int $0x80` in long mode. The DPL3 interrupt gate enters
kernel CS 0x08 using the current process's TSS.RSP0 kernel stack. The common
interrupt path saves every general register, loads kernel data selectors,
clears DF, and calls the native dispatcher. Return uses IRETQ. SYSCALL/SYSRET
and their MSRs are not configured.

| Register | Meaning |
|---|---|
| RAX on entry | Full 64-bit syscall number |
| RDI, RSI, RDX, R10, R8, R9 | Arguments 1–6, in order |
| RAX on return | Signed 64-bit result, or negative errno |
| Other GPRs, RSP, RFLAGS | Preserved across return |

Arguments are copied into the process's six native `u_arg` slots, and `u_ap`
points to those slots. `u_ar0` names the live saved frame. The dispatcher clears
`u_error` on entry and records the final error/result in the user area. It never
truncates a syscall number to select a handler.

Errors use negative values in RAX, not V7's carry-bit protocol. V7 errno values
1–34 are preserved; ENOSYS=35 is a Nexix extension. Unknown and unimplemented
numbers return -ENOSYS. The native libc wrapper converts negative errno to its
usual failure result and `errno`. Carry and direction flags are preserved;
callers must inspect RAX to recognize an error.

Handlers currently run with interrupts disabled. They can block by switching
out through process sleep/wait, which resumes them with interrupts disabled.
Pipe/TTY/wait/pause sleeps are interruptible by signals. Syscalls return EINTR
or completed byte counts; automatic restart is not implemented. Eager fork and
heap growth can require many page allocations.

The private bootstrap gate is now `int $0x81`, separate from the native ABI.
Its hooks run only while their corresponding tests are active; unknown or
inactive commands return -ENOSYS. It is not a supported user interface.

## Types and memory layout

The native platform uses eight-byte pointers, size_t, long, offsets, time values,
and tick counters; int and PIDs are four bytes. User/group IDs and mode values
are unsigned two-byte values. Exported typedefs have the `nx_` prefix.
Records use x86_64 native little-endian layout, separate from PDP-11 disk records.

User addresses currently lie in `[0x40000000, 0x800000000000)`, with explicit
4 KiB mappings. Low kernel identity mappings are supervisor-only. Passing a
pointer does not grant access to unallocated pages or automatic stack growth.
There is one execution context per process; other processes cannot alter its
live private mappings.

`struct nx_tms` contains four signed 64-bit fields at offsets 0, 8, 16, and 24:
utime, stime, cutime, cstime. Its size is 32 bytes. Counts use NX_CLK_TCK=100
PIT ticks per second. They are not wall-clock timestamps. Wait writes a signed
four-byte status, even though its pointer occupies an eight-byte argument slot.

## Implemented calls

| Number | Call | Arguments | Result |
|---|---|---|---|
| 1 | exit | status | Does not return; low eight bits become wait status bits 8–15 |
| 2 | fork | None | Child PID in parent, zero in child |
| 3, 4 | read, write | fd, buffer, count | Bytes transferred |
| 5 | open | Path, flags, optional creation mode | Lowest free descriptor |
| 6 | close | fd | Zero |
| 7 | wait | Writable int32 status pointer, or NULL | Exited or traced-stopped child PID; ECHILD if none |
| 8 | creat | Path, mode | Writable descriptor; creates/truncates |
| 9, 10 | link, unlink | Old/new paths, or path | Zero |
| 11 | exec | Pathname, argv | Replace image with empty environment |
| 12 | chdir | Path | Zero |
| 13 | time | Optional writable time_t pointer | Wall-clock seconds |
| 14 | mknod | Path, inode mode, 16-bit device number | Zero; root only |
| 15, 16 | chmod, chown | Path, mode / UID, GID | Zero |
| 17 | brk | New address, or zero to query | Zero, or current break |
| 23, 46 | setuid, setgid | ID | Zero |
| 25 | stime | Readable time_t pointer | Zero; root only |
| 27 | alarm | Seconds | Previous remaining seconds |
| 29 | pause | None | EINTR after a signal |
| 30 | utime | Path, pointer to two time_t values | Zero |
| 33 | access | Path, permission bits | Zero; real credentials |
| 34 | nice | Signed increment | Zero |
| 36 | sync | None | Zero, or EIO for a failed mount |
| 37 | kill | Signed PID/group selector, signal | Zero |
| 39 | setpgrp | None | New process group equal to PID |
| 48 | signal | Number, handler, executable restorer | Previous handler |
| 51 | acct | Existing regular-file pathname, or NULL to disable | Zero; root only; EBUSY when already enabled |
| 18, 28 | stat, fstat | Path/fd, writable nx_stat pointer | Zero |
| 19 | lseek | fd, signed offset, whence | New offset |
| 20 | getpid | None | Current PID |
| 21 | mount | Device pathname, mount-point pathname, read-only flag | Zero; root only; flag low bit selects read-only |
| 22 | umount | Block-device pathname | Zero; root only; EBUSY for active references/child mounts; EIO retains the mount |
| 24 | getuid | None | Real UID |
| 31, 32 | stty, gtty | fd, sgtty pointer | Zero |
| 41 | dup | fd | Lowest free descriptor |
| 42 | pipe | Writable int32[2] pointer | Zero; descriptors in output |
| 43 | times | Writable nx_tms pointer | Elapsed uptime ticks |
| 47 | getgid | None | Real GID |
| 54 | ioctl | fd, request, argument pointer | Zero or negative errno |
| 59 | execve | Pathname, argv, envp | Replace image |
| 60 | umask | Mask | Previous mask; new value restricted to 0777 |
| 61 | chroot | Directory pathname | Zero; root only, retains cwd |
| 64 | yield | None | Zero after allowing scheduling |
| 65 | getppid | None | Parent PID |
| 66 | geteuid | None | Effective UID |
| 67 | getegid | None | Effective GID |
| 68 | dup2 | fd, target | Target descriptor |
| 69 | fcntl | fd, command, optional value | Flags or zero |

| 70 | sigreturn | Readable nx_signal_context pointer | Restores interrupted frame |
| 71 | getpgrp | None | Current process group |

Calls 64–71 are native extensions beyond V7's 64-entry table. The separate
identity calls and wait status pointer replace V7's secondary-register returns.
Only RAX changes on normal syscall return. Successful exec installs a new entry
frame. File/pipe semantics, flag values and the 48-byte stat record are
documented in [files.md](files.md). Terminal controls and native sgtty/tchars
records are documented in [tty.md](tty.md). Signal frames, interruption, heap
and clock semantics are documented in [signals.md](signals.md).

Wait validates its output range before blocking or consuming a zombie. An
invalid non-NULL pointer returns EFAULT even when no child exists. A failed
validation leaves the child available for a later wait. The validated private
mapping remains stable across sleep under the current process ownership rules.
Fork retains task 8's eager-copy and allocation-rollback behavior; processes
holding unsupported foreign file/inode/text references receive EBUSY. Native executable
text references are supported by task 51.

An assembly call needs no stack argument block:

```asm
mov $20, %eax       # NX_SYS_GETPID
int $0x80          # signed result in RAX
```

The native libc's external assembly nx_syscall6 entry converts the SysV C
calling convention into this register ABI, including argument four in R10 and
the sixth value from the caller's stack. Its ordinary external call also orders
memory accesses around the trap. Libc startup and initial wrappers are
implemented in task 11; see [libc.md](libc.md).

## Safe user access

`kernel/uaccess.c` provides user_access_ok, copyin, copyout and copyinstr for
managed address spaces. These helpers disable interrupts across lookup and
access. They check the lower-canonical user range, addition overflow, presence,
and effective VM_USER permission at every page; copyout additionally requires
VM_WRITE. Access uses the shared kernel physical identity mapping, never an
unchecked user pointer dereference. Thus invalid syscall pointers do not rely
on CPU page-fault recovery.

Copyin/out preflight the entire range before touching the destination. On EFAULT
there is no partial write, including when a valid writable prefix is followed
by a read-only or absent page. Zero-length copies touch nothing and accept any
user address in a valid space. Kernel buffer pointers and capacities are trusted
caller inputs and must actually name sufficiently large kernel buffers.

Copyinstr checks pages as it scans and stops at the first NUL. The copied count
includes that NUL on success. It returns E2BIG if capacity is exhausted without
a terminator, or EFAULT when the next required byte is inaccessible. It may
leave partial output on failure; callers must check the result before using it.
A terminator before an inaccessible page succeeds without validating the unused
suffix. None of these helpers implements demand paging or signal recovery.
Managed user CPU faults now deliver signals; kernel and unmanaged bootstrap
fault diagnostics remain fatal. See [signals.md](signals.md).

## Validation

```sh
make TOOLCHAIN_PREFIX= test-unit      # host user-copy and existing unit tests
make TOOLCHAIN_PREFIX= smoke          # actual ring 3 ABI tests, 128 MiB
make TOOLCHAIN_PREFIX= test-syscall   # same normal boot with 16 MiB
make -j3 TOOLCHAIN_PREFIX= test        # complete regression suite
```

Ring 3 tests check all argument registers at full width, every preserved general
register, CF/DF/IF preservation, unknown/full-width syscall numbers, identity and
mask calls, read-only/unmapped/kernel/noncanonical/overflowing output pointers,
unaligned copies crossing two pages, and atomic rejection of a writable prefix.
They use the native fork ABI, force the parent to block in native wait, modify
child data and stack independently, check a high-address status pointer, collect
exit status and child accounting, and verify full cleanup. The tests use the
private gate only to inspect ABI metadata, exercise copyin/copyinstr, and confirm
that the parent has blocked; lifecycle operations use the native gate.

Host tests use independent mappings with noncontiguous physical backing and a
supervisor-only leaf. They cover invalid ranges, read-only input, permission
failures, overflow, zero length, user-range limits, and bounded strings. They
also pass AddressSanitizer and UBSan with leak detection disabled in the current
validation environment.

New success markers are NEXIX_SYSCALL_ABI_OK, NEXIX_UACCESS_OK,
NEXIX_SYSCALL_ERRORS_OK, NEXIX_SYSCALL_FORK_OK and NEXIX_SYSCALL_OK, followed by
NEXIX_BOOT_OK. Transcripts are `build/boot.log` and `build/syscall-16M.log`.
Task 10 implements ELF64 loading, exec, and argument/environment construction;
see [exec.md](exec.md). Task 11 implements native libc startup and initial wrappers; see [libc.md](libc.md).

## Header coverage (task 39)

See [native header contracts](header-coverage.md) for the complete reference
header responsibility map, native type widths and record offsets.
`make TOOLCHAIN_PREFIX= test-header-abi` checks public headers in isolation
without host libc and validates the kernel/libc record boundaries.

Task 57 implements syscall 26 (`ptrace`): request, PID, address, data, result
pointer. Reads publish a uint64 word through the fifth argument and return
zero, preserving negative words independently of negative syscall errors.
The four-argument libc wrapper clears errno and returns the word. See
[tracing](ptrace.md) for requests, the native register layout and stopped-child
wait semantics. [Task 58](debugger.md) implements requests 4/5/9 for native
instruction/data memory writes and single-step.

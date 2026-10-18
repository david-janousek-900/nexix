# Bootstrap validation environment

Initial observed boot: Debian 13, x86_64 host GCC 14.2.0, GNU binutils 2.44,
QEMU 10.0.13, GRUB 2.12, and xorriso 1.5.6. These versions record the tested
environment; they are not yet a pinned cross-toolchain or reproducible SDK.

Run `make TOOLCHAIN_PREFIX= test` in this environment. The suite checks:

- ELF64 load-segment bounds, permissions, and the Multiboot2 header.
- Successful long-mode entry and return from ten separate timer interrupts.
- Fatal invalid-opcode exception (vector 6, zero normalized error code).
- Fatal page fault (vector 14, zero read/nonpresent/supervisor error code,
  CR2 equal to the unmapped address `0x40000000`).
- VM remains alive until the test harness terminates it; unexpected exits fail.
- Host tests for bounded boot tag parsing, malformed/duplicate/extended maps,
  address overflow, overlapping reservations, partial pages, invalid frees,
  allocation uniqueness, exhaustion, and reuse.
- QEMU allocation/free self-test during regression boot. Additional 16 MiB and
  128 MiB runs exhaust physical pages, write every byte, reclaim all pages,
  restore the original free count, and preserve boot data and a loaded module.
- Paging self-test: effective kernel permissions, null guard, independent maps
  at the same virtual address, CR3 switching, TLB invalidation, executable code
  mapping, invalid inputs, mappings across page-table levels, and full private
  table reclamation without freeing caller-owned data frames.
- Hardware protection faults: write to kernel text (error 3), NX instruction
  fetch (error 17), and reads from unmapped/null pages (error 0). Each must report
  the expected CR2 address. These tests execute at CPL0, with CR0.WP enabled.
- Permission-change invalidation: cache an executable mapping, change it to NX,
  and require an instruction-fetch fault; cache a writable mapping, change it to
  read-only, and require a write-protection fault.
- Page-table exhaustion at 16 MiB: failed address-space creation and partial
  mapping allocation restore the free count and leave no mapping behind.
- Actual CPL3 execution in two address spaces with different TSS.RSP0 stacks:
  user stack writes, general-register and direction-flag preservation across
  trap return, at least ten timer IRQs per context, kernel continuation return,
  and complete test-context memory reclamation.
- User protection faults: reading supervisor kernel memory (PF error 5),
  writing RX user code (PF error 7), and executing the NX user stack (PF error 21).
  These require CS=`0x23`, SS=`0x1b`, a validated kernel entry stack, and expected CR2.
- User `cli` and port I/O cause GP error 0. User `int 0x20` cannot invoke a
  kernel-only gate and causes GP error `0x102`.
- An unmapped user RSP still allows entry through the TSS kernel stack.
  An intentionally invalid TSS.RSP0 produces a double fault with diagnostics
  on the independently guarded IST1 stack, without a triple-fault VM exit.

The serial transcripts are retained in `build/boot.log`, `build/ud.log`, and
`build/pf.log`, plus `build/memory-16M.log` and `build/memory-128M.log`.
Test images are separate from the ordinary boot image.
The smoke runner gives every full self-test boot 45 seconds, including memory
exhaustion, paging-OOM recovery and bad-stack recovery. Deliberate fatal-fault
runs retain their 15-second budget. Full boots must still emit all required
completion markers; increasing the budget does not accept partial boot output.
Additional paging logs are `build/wp.log`, `build/nx.log`, `build/unmap.log`,
`build/null.log`, `build/protect.log`, and `build/paging-oom.log`. `make TOOLCHAIN_PREFIX= test-paging`
runs the protection and allocation-failure cases separately.
`make TOOLCHAIN_PREFIX= test-user` runs the user protection and stack cases;
their transcripts are `build/user-*.log`. Regression boot must also emit all four
`NEXIX_USER_*_OK` markers. Independent groups can run concurrently with
`make -j3 TOOLCHAIN_PREFIX= test`.
The host parser/allocator tests also pass AddressSanitizer and UBSan with leak
detection disabled (LeakSanitizer cannot run under this session's ptrace).
The suite validates native ELF loading and execution, rebuilt V7 userland
(task 16), normal init/shell startup (task 18) and interactive operation
(task 19). Native V7 filesystem APIs are tested in task 12.

V7 type adaptation (task 7): `make TOOLCHAIN_PREFIX= test-unit` also runs
`build/v7-types-test`. It checks independent PDP-11 byte fixtures, rejection of
24-bit address overflow without mutation, inode-block boundaries through inode
65535, device encoding, and preservation of native addresses and offsets.
Layout assertions compile in both host tests and the kernel. See
[v7-types.md](v7-types.md) for runtime/disk widths and deferred machine features.

Process scheduling and lifecycle (task 8): every successful boot checks all eight
`NEXIX_PROCESS_*_OK` markers. `make TOOLCHAIN_PREFIX= test-process` repeats the
normal image with 16 MiB RAM and retains `build/process-16M.log`. Tests cover:

- All callee-saved integer registers across an explicit context switch,
  including preservation of an IRQ-disabled caller.
- Two kernel workers that require PIT preemption to complete a handshake.
- Three wait-channel sleepers: wake all matching sleepers, preserve unrelated
  sleepers, and return zero on repeated wakeups.
- Idle scheduler wakeup after a deadline, blocking wait, ECHILD, encoded exit
  status, cumulative accounting, and an orphan adopted by PID 1.
- All free process slots reserved and aborted, with EAGAIN at capacity and no
  leaked VM roots, plus the non-root 25-process limit.
- Ring 3 fork with independent code, stack, data, read-only, and distant mappings,
  preserved permissions and bytes, inherited credentials and signal addresses,
  rebased per-process pointers, and correct parent/child return values.
- Rejection of unsupported file references before copying any pages.
- Allocation failure at each of the sixteen allocation boundaries in that
  fork image; every failure restores the free count and leaves the parent intact.
- Parent/child user-mode timer preemption and register preservation, independent
  stack/data writes, wait status, and child accounting.
- Final recovery of every process-owned data/table page and every process slot.

The process test gate uses `int 0x81` commands only; the native ABI uses `int 0x80`. The original integer-context fixtures remain; tasks 25–27 add dedicated FP
state isolation and signal-context tests. User TLS is not implemented. See [processes.md](processes.md).

Native syscalls and safe user access (task 9): successful boot requires
NEXIX_SYSCALL_ABI_OK, NEXIX_UACCESS_OK, NEXIX_SYSCALL_ERRORS_OK,
NEXIX_SYSCALL_FORK_OK and NEXIX_SYSCALL_OK. `make TOOLCHAIN_PREFIX= test-syscall`
boots the ordinary image with 16 MiB and saves `build/syscall-16M.log`; the full
suite includes this run. `make test-process` remains available for the same
16 MiB regression boot with `build/process-16M.log` as its transcript.

The new host user-copy tests use independent mappings and noncontiguous backing,
cover full-range atomic rejection, effective supervisor/read-only permissions,
overflow and canonical-range limits, zero length, NUL termination at boundaries,
capacity exhaustion, and partial string failure. They pass AddressSanitizer and
UBSan with leak detection disabled. Ring 3 tests exercise six full-width argument
registers, every preserved GPR, CF/DF/IF, error clearing, supported identity/mask
calls, high-bit and unsupported syscall numbers, invalid output addresses,
unaligned cross-page output, and a native fork/wait/exit sequence. The child
confirms the parent is blocked in wait before exiting; an earlier invalid status
pointer leaves the child available to reap. Parent and child memory remain
independent, tick accounting is collected, and all process resources are freed.
See [syscall-abi.md](syscall-abi.md) for the ABI and exact service scope.

ELF64 execution (task 10): every successful boot requires `NEXIX_ELF_OK`,
`NEXIX_EXEC_ROLLBACK_OK` and `NEXIX_EXEC_OK`. `make TOOLCHAIN_PREFIX= test-exec`
runs the complete regression boot with 16 MiB RAM (`build/exec-16M.log`), including
malformed ELF rejection, partial-allocation rollback, argument/environment
stacks, and an actual CPL3 fork/exec/wait sequence. See [exec.md](exec.md).
The full suite now uses test-fs for its low-memory regression boot; test-process,
test-syscall, test-exec and test-libc remain available separately.

Native libc (task 11): every successful boot requires `NEXIX_LIBC_ABI_OK` and
`NEXIX_LIBC_OK`. `make TOOLCHAIN_PREFIX= test-libc` runs the regression boot with
16 MiB RAM (`build/libc-16M.log`), including a real C program linked with native
crt0/libc, argument/environment startup, six-argument register conversion,
callee-saved registers, errno translation, fork/exec/wait, strings, exit
callbacks and page/process cleanup. Empty and maximum-size startup vectors
are also exercised. See [libc.md](libc.md) for the supported interface.

Block devices and V7 filesystem (task 12): successful boots require
`NEXIX_BLOCK_OK`, `NEXIX_V7FS_READ_OK`, `NEXIX_V7FS_ALLOC_OK`,
`NEXIX_V7FS_WRITE_OK`, `NEXIX_V7FS_EXEC_OK` and `NEXIX_V7FS_OK`.
`make TOOLCHAIN_PREFIX= test-fs` runs the complete regression boot with 16 MiB RAM
(`build/fs-16M.log`). Host filesystem tests run under test-unit against the
generated `build/root-test.img`. Tests cover lookup, all indirect levels,
sparse reads/writes, free-list exhaustion/reuse, remount persistence, ELF
execution from a filesystem file, malformed metadata and device fault handling.
See [filesystem.md](filesystem.md) for API coverage and remaining work.


File descriptors and pipes (task 13): successful boots require `NEXIX_FILES_OK`,
`NEXIX_PIPE_OK` and `NEXIX_FILES_CLEANUP_OK`. `make TOOLCHAIN_PREFIX= test-files`
runs the native C acceptance fixture with 16 MiB RAM; normal smoke uses 128 MiB.
Coverage includes descriptor exhaustion, shared offsets across fork/dup, dup2,
flags, creation/umask, append/truncation, namespace operations and open-unlinked
lifetime, stat/cwd, permission errors, close-on-exec, failed exec preservation,
pipe transfer/EOF/EPIPE and deterministic blocked-writer wakeup. Repeated runs
restore all file/inode/pipe pools and exact physical-page/free-block/free-inode
counts. Host tests cover permission selection and directory search, complete
indirect-tree reclamation, inode exhaustion/reuse, directory growth, remount
and read-only mutation rejection. See [files.md](files.md).


Serial TTY (task 14): every successful boot requires `NEXIX_TTY_USER_WRITE_OK`,
`NEXIX_TTY_OK` and `NEXIX_TTY_CLEANUP_OK`. Host tests cover cooked editing/echo,
record boundaries and EOF, queue capacity/wrap/reuse, raw/cbreak, CR/tab expansion,
baud/parity selection and ioctl validation before mutation. Native userland tests
cover terminal descriptors, blocking read wakeup, read-only mount output,
controlling-terminal resolution, ioctl/stty/gtty/isatty, invalid buffers without
consumption, fork/exec inheritance and complete resource cleanup.
`make TOOLCHAIN_PREFIX= test-tty` exchanges cooked, raw and cbreak bytes through
real COM1 IRQ input with 16 MiB and 128 MiB RAM. It stops a writer with Ctrl-S,
confirms the writer sleeps, then wakes it with Ctrl-Q. Test-only input injection
is disabled for this UART pass, which requires `NEXIX_TTY_UART_OK`. The target
also sends lines to the interactive demo and checks its response/quit behavior,
requiring `NEXIX_TTY_DEMO_OK`. An additional PTY check verifies the interactive
stdio multiplexer and Ctrl-A X exit (`build/tty-pty.log`). See [tty.md](tty.md).


## Task 15 acceptance

`make TOOLCHAIN_PREFIX= test-signals` boots the complete tests at 16 and 128 MiB.
The ring-3 fixture verifies signals and handler return, invalid privilege frames,
nested and nonlocal handler exits, alarms, interrupted I/O/wait, faults and
SIGPIPE, heap OOM rollback, credential/metadata rules and exact page/reference
cleanup. Host TTY tests check interrupt/quit controls and RAW preservation.
See [signals.md](signals.md) for the ABI, coverage and success markers.

## Task 16 acceptance

`make TOOLCHAIN_PREFIX= test-v7` verifies native V7 userland at 16 MiB and
128 MiB. Logs are `build/v7-16M.log` and `build/v7-128M.log`.
Required markers are `NEXIX_V7_INIT_ELF_OK`, `NEXIX_V7_USER_PROGRAM_OK`,
`NEXIX_V7_SHELL_OK`, `NEXIX_V7_UTILITIES_OK` and
`NEXIX_V7_USERLAND_CLEANUP_OK`. Init is checked for successful ELF loading;
the shell and utilities are actually executed at CPL3. The fixture runs twice
with fresh writable roots and checks process/resource reclamation. See
[userland.md](userland.md) for coverage and remaining startup work.

## Task 17 acceptance

`make TOOLCHAIN_PREFIX= test-rootfs` checks the packaged root image on the host
and boots QEMU at 16 MiB and 128 MiB. `NEXIX_ROOT_IMAGE_OK` and
`NEXIX_ROOT_ELF_OK` confirm layout/device checks and loading all eleven executables
read from the volume. Host tests check file bytes, directory link counts,
complete block ownership/free-list accounting, spare inodes, reproducibility
and insufficient-space rejection. See [root-image.md](root-image.md).

## Task 18 acceptance

`make TOOLCHAIN_PREFIX= test-boot` boots the normal production ISO at 16 MiB
and 128 MiB. It checks PID 1 startup, PID 2 shell commands, writable root I/O,
execution of a copied program and init restarting the shell after console EOF.
Component tests now use `test=selftest` rather than starting init. They require
`NEXIX_ROOT_EXEC_USER_OK`, `NEXIX_ROOT_EXEC_OK` and
`NEXIX_ROOT_EXEC_CLEANUP_OK` in addition to earlier markers. The native driver
checks relative exec, missing catalog-only names, permission/type rejection,
V7 set-ID exec, credential rollback, malformed ELF and post-exec cleanup. Injected loader
read failures verify rollback before and after allocating staged pages.
See [booting.md](booting.md) for commands and transcripts.

## Task 19 acceptance

`make TOOLCHAIN_PREFIX= test-shell` runs 271 command checks per production boot
at 16 MiB and 128 MiB, using real COM1 input. It exercises line editing,
foreground/pipeline interrupts, quit, traps, EOF, background commands, scripts,
globbing, 5120-byte expansion and a longer fork/exec/wait session. Native root
regression tests additionally check `/dev/null` read/write/stat/seek and invalid
user-buffer rejection. Host and kernel image checks verify the new device inode.
See [interactive-shell.md](interactive-shell.md) for coverage and limitations.

## Task 20 acceptance

`make TOOLCHAIN_PREFIX= test-storage` boots disposable IDE roots three times
at each of 16 MiB and 128 MiB. It verifies persisted contents and hard links,
append/unlink, execution of copied ELF files through single-indirect blocks,
allocation after deletion, restored free-block/inode totals and invalid-root
rejection. Logs are `build/storage-{16M,128M}.log`.
`make TOOLCHAIN_PREFIX= test-unit` also runs the simulated-port IDE driver
checks for timeout, absence, unsupported disks, data transfer, range errors
and failed reads/cache flushes. See [storage.md](storage.md).


## Task 21 acceptance

`make TOOLCHAIN_PREFIX= test-inventory` checks the V7 manifest without kernel
builds or QEMU. It covers all 2,144 full-distribution reference files, checks content hashes
against the pinned archive index, validates
component identities and dependency/evidence references, and checks implemented
outputs against build and root-image packaging declarations. Negative tests
exercise missing ownership, concealed components, duplicate IDs, missing
requirements, unsupported implementation claims and absent evidence.
See [porting-inventory.md](porting-inventory.md). This verifies inventory coverage,
not completion of the remaining ports or their transitive API audits. Additional
negative checks cover omitted installed scripts, binary-only claims, installed
program-index gaps and provenance corruption. The original archive can be checked
independently with `python3 tools/import_v7_reference.py /path/to/v7.tar.gz --check`.

## Task 22: RTC system clock

`make TOOLCHAIN_PREFIX= test-rtc` checks BCD/binary and 12/24-hour decoding,
midnight/noon, Gregorian leap dates, century fallback, invalid fields, battery
validity, stuck update-in-progress, inhibited updates and unstable snapshots.
The host test also verifies PIT advancement, `stime` validation and preservation
of the previous time after rejected updates.

A bounded QEMU boot uses `-rtc base=2024-02-29T12:34:56,clock=vm` and checks the
reported epoch against 1709210096 (allowing ten seconds for firmware/boot),
`NEXIX_RTC_OK`, absence of panic and the normal shell prompt. Its log is
`build/rtc-boot.log`. `make TOOLCHAIN_PREFIX= test-signals` exercises the complete
self-test boot at 16M/128M, including userspace RTC-derived `time`, pointer-copy
validation, privileged `stime`, and existing signal/service regressions.

The initial PC platform uses CMOS century register 0x32. An unavailable century
byte (0 or 0xff) maps years into 2000–2099; other malformed values are rejected.
RTC snapshots must match, with UIP clear and valid battery-backed contents.
Polling is bounded; failures leave epoch-plus-uptime and print a diagnostic.
Reads run before timer IRQs are enabled and never alter RTC date registers.
The RTC must hold UTC. QEMU's [RTC options](https://qemu-project.gitlab.io/qemu/system/invocation.html)
provide UTC or fixed-date initialization. There is no ongoing resynchronization
or RTC writeback in this task. Filesystem timestamp updates are implemented in
task 23; timezone conversion is implemented in task 24. V7 disk formats are unchanged.

## Task 23: persistent V7 timestamps

`make TOOLCHAIN_PREFIX= test-unit` includes deterministic-clock filesystem
checks for creation, overwrite without growth, reads/EOF, zero-count I/O,
path lookup, parent directories, hard links, unlink, truncation, metadata
changes and explicit utime-style values. Tests verify the encoded disk bytes,
remount persistence, read-only byte-for-byte preservation, superblock sync
time and timestamp write-error handling.

`make TOOLCHAIN_PREFIX= test-signals` checks native utime and ctime together
with existing permission and read-only behavior at 16M/128M.
`make TOOLCHAIN_PREFIX= test-storage` performs three cold boots at each memory
size on disposable IDE roots with consecutive fixed UTC RTC dates. An
independent parser reads atime/mtime/ctime directly from V7 disk bytes after
creation and after read/write/unlink operations on the next boot. The third
boot verifies `ls -l` displays the persisted modification date before reading
the file, then checks block/inode reclamation. Logs are `build/storage-16M.log`
and `build/storage-128M.log`. Seed image timestamps remain reproducibly zero;
existing files acquire times as they are accessed or modified.

Interactive shell regression coverage includes repeated empty, whitespace-only
and comment-only prompt lines, queued empty lines, carriage-return/CRLF input,
and repeated Ctrl-D at the shell prompt followed by a command in the replacement
single-user shell. Ctrl-D with no pending text is EOF and exits the current V7
shell; init runs /etc/rc and returns to single-user startup while getty/login are
not installed. Ctrl-D after pending text only delivers that text; it does not
insert a newline or permanently close the terminal.

`make TOOLCHAIN_PREFIX= test-shell` exercises RAM and disposable IDE roots at
16M/128M. IDE logs are `build/interactive-16M-ide.log` and
`build/interactive-128M-ide.log`; the working `build/nexix.img` is never changed.
If input becomes unresponsive under `make run`, QEMU's monitor can be reached
with Ctrl-A then C. Capture `info registers` and `info pic` before rebooting to
help distinguish guest interrupt/scheduler failures from host terminal input
problems. Ctrl-A then C returns to the guest terminal.

## Task 24: timezone libc and V7 date/touch

`make TOOLCHAIN_PREFIX= test-time` checks native time conversion against fixed
calendar values: epoch and negative times, leap days/centuries, fractional-hour
offsets, northern/southern DST transitions, V7 US 1974/1975 exceptions, TZ
changes, malformed TZ fallback and ftime fields. The host harness renames the
native symbols so its checks cannot accidentally call host time conversion.

QEMU IDE-root acceptance runs at 16M/128M with a fixed RTC. It checks `date -u`,
local timezone names, spring DST, root date setting, omitted date fields, 24:00
rollover, malformed-input status, `touch` creation/read-rewrite/content retention
and `-c` diagnostics. An independent decoder validates native wtmp old/new
time-setting records. Logs are `build/time-16M.log` and `build/time-128M.log`.
`test-signals` also executes native `date` under an unprivileged UID and requires
exit status 1. `test-storage` now touches files on two consecutive cold boots
and verifies their timestamps and contents survive the third.

`test-root-image` verifies all eleven native executables against their ELF files;
`test-boot` checks normal shell startup and the expanded /bin listing. Existing
IDE disks retain their previous userland. A separate `DISK_IMAGE` selects a new
root without replacing the working disk; full disk upgrades remain task 357.


Task 47 acceptance: `make test-terminal` runs bounded host fixtures, native
terminal/account records in the 16/128 MiB signal/stdio boots, and real COM1
password input at 16/128 MiB. It checks password echo suppression, SIGINT and
terminal-state restoration, utmp field boundaries and descriptor cleanup.
See [terminal/account contracts](terminal-accounts.md).


Task 48 acceptance: `make test-system` checks V7 system through host fork/exec/
wait failure fixtures and the real native shell at 16/128 MiB, including raw
statuses, signal inheritance/restoration, environment/cwd and repeated cleanup.
See [system contracts](system.md).


Task 49 acceptance: `make test-popen` combines host setup/wait/flush failure
fixtures with native 16/128 MiB shell pipes, concurrent-child statuses, EOF,
closed-stdio descriptor collisions and broken-pipe cleanup. See
[process-pipe contracts](popen.md).


Task 50 acceptance: `make test-setid` uses filesystem-loaded credential probes
at 16/128 MiB to verify real/effective IDs, V7 root and tracing exceptions,
fork/ordinary-exec inheritance, permission checks and failed-exec rollback.
Test-only physical page reservation forces allocation failure after staging
starts. Complete page/descriptor/inode/block cleanup is checked; persistent
storage is preserved. See [set-ID contracts](setid-exec.md).


Task 51 acceptance: `make test-text` checks the actual text table's capacity,
reference/cache lifetimes and errors, plus synchronized filesystem exec probes
at 16/128 MiB for ETXTBSY, aliases, fork/exec/exit and sticky ownership. Existing
set-ID rollback and complete native resource reclamation remain checked. See
[text ownership](text-ownership.md).


Task 52 acceptance: `make test-core` verifies native ELF register/FPU and
mapped data/heap/stack snapshots, signal/core status, credential/file protections,
readonly/ENOSPC handling and resource cleanup at 16/128 MiB. Host error fixtures
and an independent Python/readelf decoder validate the format. `make test-shell`
checks the actual V7 core message and file on disposable RAM/IDE roots at both
memory sizes. See [core format and policy](core-dumps.md).

## Task 57 tracing acceptance

`make test-ptrace` runs disposable-root tracing acceptance at 16/128 MiB and
the inventory, header and libc export audits. See [ptrace.md](ptrace.md) for
the request/register ABI and exercised cases. Markers `NEXIX_PTRACE_OK` and
`NEXIX_PTRACE_ORPHAN_OK` require both user debugger operations and bootstrap
verification of a stopped orphan's release/reap, followed by the existing
root cleanup and boot markers. `make test-unit` supplies the host baseline.

## Task 58 debugger writes and stepping

`make test-debugger` runs executable-alias user-copy tests, disposable-root
QEMU acceptance at 16/128 MiB and inventory/header/export checks. The required
`NEXIX_DEBUGGER_OK` marker covers instruction/data writes, shared/sticky text
rejection, private patches/fork inheritance, unchanged executable files, RX/NX
protection, software breakpoints and stepping across normal instructions,
syscalls, fork, signals, faults and exec. Existing root cleanup checks remain
active. See [debugger.md](debugger.md) for native adaptations.

## Task 59 profil sampling

`make test-profile` combines host golden bucket/bounds/fault tests, disposable
16/128 MiB QEMU runs and inventory/header/export checks. `NEXIX_PROFILE_OK`
requires deterministic real-VM samples, user PIT and trap-return system
charges, fork/private counters, failed/successful exec behavior and quiet
fault disabling, followed by existing cleanup/boot markers. Private fixture
hooks provide scale inspection, sample injection and kernel-residence/sleep
intervals only while root-exec tests are active. See [profiling.md](profiling.md).

## Task 60 monitor runtime and profiling startup

`make test-monitor` combines host scale/format/capacity/error fixtures, native
16/128 MiB QEMU runs and inventory/header/export checks. Required marker
`NEXIX_MONITOR_OK` covers real -pg instrumentation, mcrt0/fmcrt0, precise call
counts and histogram samples, integer/FP/variadic register preservation,
manual monitor, versioned file decoding, stdio/atexit ordering and fork/exec/
_exit behavior. Existing root resource checks remain active. See
[monitor.md](monitor.md) for the native call convention and output records.

Task 68 adds [V7 serial packets](serial-packets.md), TIOCSETD/DIOCGETP
negotiation and libc pkon/pkoff/turnon. Run `make test-packet` for host
protocol/ownership/UART/libc checks and real QEMU COM1 exchanges at 16/128 MiB.

Task 69 implements [native memory/special devices](memory-devices.md), including
privileged mem/kmem byte access, null aliases and controlling-TTY acceptance.
Run `make test-memdev` for host and 16/128 MiB syscall tests.

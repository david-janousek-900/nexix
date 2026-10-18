# V7 kernel source responsibility audit (task 38)

This audit accounts for every one of the 113 supplied files under
`reference/v7/usr/sys` and all 64 entries in the original `sysent` table.
The canonical per-file mapping is [kernel-coverage.json](kernel-coverage.json).
Each source appears exactly once, including headers, recipes, configuration
inputs and historical binary objects/archives. Groups identify native integration
points, responsibilities and roadmap owners. The reference tree remains pristine.

Completing this audit does **not** complete the kernel port. The inventory entry
`distribution.kernel` remains `not_ported`; kernel behavior still needs the
feature tests and consumer acceptance assigned below. Existing native paths are
implementations of subsets, not claims that every function or field in their
original files is adapted. Task 39 must audit header fields and exported ABI
contracts; task 355 and task 359 close whole-distribution integration.

## Existing foundation and remaining owners

| Original area | Native foundation | Remaining acceptance / tasks |
|---|---|---|
| m40/mch/l machine assembly, segmentation, memory allocation | Long mode, native interrupt frames, paging, resident memory, eager fork, FXSAVE64 | phys implemented by 64; runtime assembly contracts 79; no PDP-11 binary execution or disk-backed swap required |
| main/slp/clock | Boot, scheduler, sleep/wakeup, PIT ticks, RTC wall clock, alarms, user/system CPU times | CPU usage saturation and priority decay 362; general timeout callouts 364; profil sampling implemented by 59 |
| alloc/iget/nami/rdwri/fio/bio | V7 disk encodings, inode/sector cache, lookup, permissions, files, timestamp updates | Executable exclusion implemented by 51; raw I/O 53; mount/umount 54–55 |
| sys1–sys4/sysent/trap/ureg | Native dispatcher, user-copy validation, process/files/identity/time calls | Set-ID exec implemented by 50; core dumps implemented by 52; ptrace implemented by 57–58; pending syscall slots below |
| text | Native ELF images, shared inode ownership, ETXTBSY and sticky ownership entries | Task 51 complete; eager private VM pages replace physical text sharing/swap caching; private debugger modifications implemented by 58 |
| sig | V7 signals, saved registers/FP frames and native ELF core dumps | Core policy/format implemented by 52; stop/wait/register access implemented by 57; memory changes and stepping implemented by 58 |
| pipe | Native bounded pipes, sleep/wakeup, SIGPIPE and descriptor lifetime | Existing files tests; consumer acceptance remains distribution-wide |
| acct | Process exit integration point | Accounting records, privileges, storage errors 56 |
| prim/mx1/mx2/fakemx | Native group/channel ownership, record I/O, bounded queues, flow control and open notifications | Ownership/data/TTY bridging and libc implemented by 65–67; independent terminals remain 89–93; historical optional stub does not satisfy functionality |
| tty/dc/dh/dz/kl/partab | COM1 cooked/raw/cbreak, process groups and supported parity/tab flags | Terminal equivalents and TANDEM/LCASE/delay/parity contracts 74; independent ports 89–93 |
| pk0–pk3 | Native framing/checksums, negotiation, windows/retries, EOF/CLOSE, pinned ownership and IRQ UART/TTY/mpx integration | Task 68 complete; [packet contract](serial-packets.md); UUCP and additional serial hardware retain their own acceptance tasks |
| dn/dhdm/dhfdm/du | Serial integration point only | Modem/dial control 75 |
| hp/rf/rk/rl/rp | Native disk views, source partitions/units, bounded recovery, ATA media and overlapping mount protection | Tasks 53/70 complete; [controller contracts](disk-controllers.md); queue/interleave acceptance remains 73 |
| ht/tm | Sequential records, marks, rewind/spacing and pinned regular-file media | Task 71 complete; [tape contracts](tape-devices.md) |
| tc | Block integration points only | DECtape 72 |
| dsort/dkleave | Block integration point only | Queue ordering/interleave 73 |
| mem/sys | Byte-addressed memory/null devices and controlling-TTY forwarding | Task 69 complete; [native access rules](memory-devices.md), host and 16/128 MiB syscall acceptance; independent TTYs remain 89–93 |
| cat, vp/vs | Future native endpoints | Typesetter 76; printer/plotter protocols 77 |
| prf/subr | Serial diagnostics and native memory/user-copy helpers | Helper/consumer contracts 39/79; memory device consumers 69 |
| h/* | Native runtime types and explicit historical disk encoders | All header/field/consumer contracts 39 plus their feature owners |
| conf/*, LIB1/LIB2/mklib | Current Makefile/bootstrap | Native configuration/recipes 78; supplied binaries are provenance |

## System-call audit

The JSON records each slot's original routine and one of three classifications:
`native_path` means dispatch or a native library path exists; `pending` means
required functionality is absent or incomplete; `historical_stub` means the
original slot itself used `nosys` or `nullsys`. These classifications describe
responsibility, not complete semantic equivalence. Native calls also have
extensions (for example process-group calls); original reserved numbers must
not be treated as missing required features merely because they were stubs.

| Original slot | Required behavior | Owner |
|---:|---|---:|
| 21 | mount | 54 |
| 22 | umount | 55 |
| 26 | ptrace | 57, then 58 |
| 35 | ftime subsecond snapshot | 363 |
| 44 | profil sampling implemented | 59; native startup/monitor implemented by 60 |
| 51 | acct | 56 |
| 52 | phys implemented | 64; native page windows, ownership and rollback |
| 53 | lock implemented | 63; resident memory, V7 privilege and lifecycle rules |
| 56 | mpx ownership, data/control, TTY bridging and libc implemented | 65–67; independent terminal hardware 89–93 |

`ftime` currently uses libc's `time` path and returns zero milliseconds. A native
clock snapshot is required; merely having a library symbol does not finish it.
V7's clock increments a saturating CPU counter and applies `SCHMAG` decay every
second before recalculating user priority. Nexix currently selects runnable
processes by priority but derives user priority from nice alone. The general
V7 `timeout` callback queue is also absent: alarm deadlines do not replace it.
These findings add tasks 362–364 without renumbering existing tracking IDs.
They are prerequisites to final acceptance, despite their higher IDs.

## Validation and limits

Run `make TOOLCHAIN_PREFIX= test-kernel-coverage` or `test-inventory`.
The checker compares the pinned inventory and actual reference directory with
the source groups, rejects omissions/duplicates, checks native paths and roadmap
owners, and compares all 64 original routine names directly with `sysent.c`.
Pending syscall owners cannot be marked Done. Negative tests cover source and
inventory omissions, unsafe paths, invalid owners, missing slots, routine drift,
mislabelled historical stubs and false closure.

The checker validates audit integrity; it does not execute historical code,
prove function-by-function equivalence, or replace runtime acceptance tests.
No kernel executable behavior changes in task 38, so QEMU regressions are not
needed for this audit-only change. Existing host unit tests are run as a baseline.

Task 59 implements [native V7 profil sampling](profiling.md), including user
PIT samples and per-process system-time charges at trap return. Profiling
startup/monitor records are implemented by task 60; the kernel itself is not profiled.

Task 62 supplies [versioned kernel-information snapshots](kernel-information.md)
for process/file/inode/text/mount tables, managed memory and cumulative I/O.
Diagnostics use these explicit records in place of historical kernel offsets;
ps, pstat and iostat still need their separate command-port acceptance.

Task 63 implements [V7 lock(53)](resident-lock.md): effective-root permission,
SULOCK state and fork/exec/exit rules. Native memory remains resident throughout.

Task 64 implements [native phys mappings](physical-mappings.md), with explicit
owned RAM/device references and shared fork/exec lifetime.

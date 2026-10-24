# Nexix tasks

Project overview and build instructions: [README](../README.md).

Version **0.1.0** ports the complete V7 source distribution to native x86_64:
the kernel's applicable behavior, userland, libraries, development tools,
scripts, manuals, supporting data and standalone facilities. It includes
multi-user startup, RTC-derived time and correct persistent V7 timestamps.

### Current implementation and scope

The roadmap review baseline is commit `fa6076d`. Current fresh installations boot
to multi-user V7 login with persistent IDE storage. 103 native ELF programs are
installed; the complete list and destinations are in [the installation manifest](native-install.json).
Native libc/libm archives, four startup variants and public headers are installed;
see [development interfaces](development-interfaces.md).
The inventory accounts for **225 components, 2,144 distribution files and
36 supporting/platform requirements**. Its component statuses are 116 implemented,
4 partial, 0 prototype and 105 not ported; importing reference inputs does not
complete their native ports.

The expanded roadmap below has **455 tracking tasks: 271 done, 0 in progress
and 184 to do**. Tasks **1–108, 119–165, 168–202, 219, 223, 226, 237, 240, 251, 253, 261–262, 264–269, 281–282, 286–287, 295, 351, 357, 362–388, 410–435, 444–449** are complete. These counts are a work breakdown, not an
estimate of the percentage of porting effort completed. Tasks 1–24 retain
their previous scope. Completed portions of the old task 25 are separated
into tasks 25–36; its remaining work is listed individually.

Release scope and agreed native adaptations:

- Preserve original reference sources and licensing; active ports and replacements
  belong outside `reference/v7`.
- Replace PDP-11 assembly, segmentation, executable formats and hardware interfaces
  with documented native equivalents that preserve applicable capabilities.
- Retain resident memory and eager fork; disk-backed swapping is not required.
- Retarget the supplied C and Fortran tools and rebuild their supported V7 sources
  inside Nexix. Host GCC may continue building the modern Nexix kernel and libc;
  complete Nexix self-hosting is outside 0.1.0.
- Keep all 13 binary-only program components as explicit release blockers until
  sources are recovered or an alternative is individually approved.
- Preserve V7 atime/mtime/ctime semantics and disk encodings; there is no separate
  birth-time field or planned timestamp-format extension.
- Restore V7 serial communications and UUCP in 0.1.0. TCP/IP belongs to 0.2.0.

Known gaps include complete TTY modes, additional devices, the native toolchain
and the remaining userland ports. The default embedded RAM
root remains 512 KiB with 96 inode slots; task 84 supports larger external images.
Task 85 separates the full IDE root; task 86 completes per-component guest test staging.

See the [porting inventory](porting-inventory.md) and its
[component manifest](v7-userland-manifest.json) for original inputs,
outputs, dependencies, acceptance checks and missing sources. The
[roadmap coverage map](roadmap-coverage.json) and
[coverage contract](roadmap-coverage.md) connect every component and
requirement to these task IDs; test-inventory validates the relationships. Older task
numbers in detailed documents refer to the previous roadmap and are mapped below.

### Implementation, testing and commit rules

Each future task follows one implementation, test and commit cycle:

1. Implement the stated capability.
2. Run focused acceptance tests and affected regressions.
3. Update the inventory, documentation and task status.
4. Commit the task and record its hash and test evidence.

**Done** means implemented, tested and committed. **In progress** means relevant
implementation exists but complete stated coverage is unfinished. **To do**
includes blocked work, with the blocker recorded in notes.

Historical commits sometimes contain several features. The tables record their
actual shared commits; the expanded numbering does not rewrite Git history.
Numbers are stable sequential tracking IDs. Execute dependency-ready tasks;
source recovery and independent ports may proceed before lower-numbered
dependencies finish.

Each component must reference its owning tasks, prerequisites, native outputs,
acceptance checks and commits. Every source, header, assembly file and build
input must map to an adaptation, supporting asset or documented native equivalent.
Compilation alone, preserved binaries or an ENOSYS stub do not complete a port.

Unless a row states otherwise, a program-port task includes native build
integration, installation, source-defined behavior, exit/error cases and guest
acceptance fixtures. Cover all applicable outputs, including scripts, private
helpers, libraries, generated data and optional targets. Large components are
split into smaller tasks below. Most tasks restore V7 functionality; native
ELF/x86_64 ABI and device equivalents are platform adaptations.

### Previous roadmap cross-reference

| Previous roadmap | Expanded tasks |
|---|---|
| 1–24: implemented foundation | 1–24 |
| 25: kernel/libc prerequisites and floating point | 25–82 |
| 26: independent terminals | 89–93 |
| 27: multi-user startup | 94–108 |
| 28: full userland/toolchain port | Component tasks in 87–88 and 109–355 |
| 29: packaging and release | 83–86 and 356–361 |
| Roadmap and coverage maintenance | 37 |

### Implemented foundation

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 1 | Preserve V7 reference sources and licensing | Done | `e317f01`; full archive corrected in `a856583` |
| 2 | GRUB bootstrap and x86_64 long mode | Done | `e317f01` |
| 3 | Serial console, exception entry and PIT interrupts | Done | `e317f01` |
| 4 | Boot memory-map parsing and physical-page allocation | Done | `e317f01` |
| 5 | Protected paging and independent address spaces | Done | `09ce60d` |
| 6 | User mode, TSS and guarded kernel stacks | Done | `ca9466e` |
| 7 | Native core types and historical disk encodings | Done | `8b3f883` |
| 8 | Scheduling, sleep/wakeup, fork, exit and wait | Done | `1d6cc90` |
| 9 | Syscall ABI and safe user-memory access | Done | `342e0fe` |
| 10 | ELF64 loading and atomic exec | Done | `92daf2b` |
| 11 | Native crt0 and basic libc/syscall wrappers | Done | `2a7ce1f` |
| 12 | RAM block device and V7 filesystem foundation | Done | `366fe7c` |
| 13 | File descriptors, namespace operations and pipes | Done | `a9a0f76` |
| 14 | COM1 TTY and line discipline | Done | `1f8e4af`; COM2 not implemented |
| 15 | Signals and init/shell service interfaces | Done | `b62f694`; not complete kernel syscall coverage |
| 16 | Native init, sh and initial utilities | Done | `8783cf2`; single-user init only |
| 17 | Generate and validate a native root image | Done | `c5f6d47`; current image contains 11 programs |
| 18 | Mount root and start PID 1/shell | Done | `614f53c` |
| 19 | Interactive shell acceptance and console fixes | Done | `e6614f7`; EOF/empty-line regressions `4aa8630` |
| 20 | Persistent IDE root and cold-boot acceptance | Done | `fbbea35` |
| 21 | Complete distribution inventory and provenance checks | Done | `3d2ecb3`, `e67af65`, `a856583`; 217 components |
| 22 | UTC wall clock from RTC | Done | `14c8e67`; boot synchronization, not periodic resync |
| 23 | Persistent V7 atime/mtime/ctime | Done | `bb13519`; no birth-time extension |
| 24 | TZ-aware time conversion and native date/touch | Done | `e8ec6dd` |

### Completed portions of old task 25

These tasks are complete within their documented native contracts; full libc/libm
coverage and installation remain subject to later acceptance tasks.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 25 | Initialize and isolate x87/MMX/SSE/SSE2 state | Done | `8119d4f`; defined native contracts, not full task 25 |
| 26 | FP state across scheduling, fork and exec | Done | `8119d4f`; defined native contracts, not full task 25 |
| 27 | FP signal contexts and setjmp/longjmp controls | Done | `8119d4f`; defined native contracts, not full task 25 |
| 28 | Binary64 helpers and decimal conversion/formatting | Done | `8119d4f`; defined native contracts, not full task 25 |
| 29 | Adapt all 16 supplied V7 libm C sources | Done | `8119d4f`; defined native contracts, not full task 25 |
| 30 | mknod and special-inode lifecycle | Done | `8119d4f`; defined native contracts, not full task 25 |
| 31 | Privileged directory link/unlink semantics | Done | `8119d4f`; defined native contracts, not full task 25 |
| 32 | Per-process chroot | Done | `8119d4f`; defined native contracts, not full task 25 |
| 33 | Native scanf family | Done | `5098483`; implemented subset |
| 34 | Stream helpers and pushback | Done | `5098483`; implemented subset |
| 35 | Character/string/integer helpers and formatting extensions | Done | `5098483`; implemented subset |
| 36 | Buffered stdio, setbuf and exit/close flushing | Done | `63e8d0d`; old 25.8 |

### Remaining kernel and libc prerequisites

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 37 | Publish renumbered roadmap and enforce component-to-task coverage | Done | test-inventory validates 217 components, 35 requirements and 361 tasks; commit recorded in Git history |
| 38 | Complete kernel source adaptation coverage | Done | [Source and syscall audit](kernel-coverage.md); test-kernel-coverage; Git history; kernel port remains incomplete |
| 39 | Complete exported native V7 header/ABI coverage | Done | [Header contracts and deferred owners](header-coverage.md); test-header-abi; Git history; feature implementations remain with their tasks |
| 40 | Finish libc scalar/byte helpers and conversion contracts | Done | [V7 helper contracts](libc-helpers.md); test-libc-helpers and test-libc-interfaces; Git history; libc remains partial |
| 41 | Adapt internal FILE callers and historical remote printf convention | Done | [Native stdio adapters](stdio-compat.md); test-stdio-compat; Git history; whole consumer ports remain pending |
| 42 | Temporary-file routines | Done | [V7 mktemp contract and consumers](mktemp.md); test-mktemp; Git history; consumer ports remain pending |
| 43 | Complete error messages and perror interfaces | Done | [V7 error interfaces](errors.md); test-perror; Git history; original 0–34 messages plus ENOSYS extension |
| 44 | Password database interfaces | Done | [V7 passwd interfaces](passwd.md); test-passwd; Git history; authentication and multi-user setup remain pending |
| 45 | Group database interfaces | Done | [V7 group interfaces](group.md); test-group; Git history; account configuration and credential-changing programs remain pending |
| 46 | V7 crypt/password primitives | Done | [V7 DES/crypt contracts](crypt.md); test-crypt; Git history; known-answer vectors and all 4096 salts verified |
| 47 | getpass and tty/account lookup | Done | [Terminal/account contracts](terminal-accounts.md); test-terminal; Git history; native lookups and real COM1 echo/signal restoration at 16/128 MiB |
| 48 | system | Done | [V7 system contract](system.md); test-system at 16/128 MiB and host failure fixtures; Git history; raw wait status and historical wait-any/EINTR behavior |
| 49 | popen/pclose | Done | [V7 process-pipe contracts](popen.md); test-popen at 16/128 MiB and host failure fixtures; Git history; matching/cached child statuses, EOF and descriptor cleanup |
| 50 | Set-UID/set-GID executable semantics | Done | [V7 credential rules](setid-exec.md); test-setid at 16/128 MiB; Git history; root/tracing exceptions, effective-ID commit and failed-exec rollback |
| 51 | Executable text ownership and ETXTBSY behavior | Done | [Native V7 text ownership](text-ownership.md); test-text at 16/128 MiB and host table/error fixtures; Git history; exec/fork/exit, aliases and sticky lifetimes |
| 52 | Native core dumps | Done | [ELF core format and V7 policy](core-dumps.md); test-core and test-shell at 16/128 MiB; Git history; user registers/FPU/memory, credential restrictions and completed-dump wait bit |
| 53 | Raw block/character storage devices | Done | Git history; bounded device table, block/raw transfers, alignment, errors and descriptor lifetime; [mounted-device write guard and native raw-I/O contract](storage-devices.md); `make test-storage` |
| 54 | Multi-volume namespace and mount | Done | Git history; [V7 mount/namespace contract](mounts.md); device-qualified inode ownership, nested/overlaid mount crossings, dotdot/chroot, read-only volumes and EXDEV; `make test-mount` |
| 55 | Unmount and busy-volume handling | Done | Git history; [V7 unmount contract](unmount.md); descriptor/cwd/root/text and child-mount busy checks, targeted sticky purge, sync-error retention, remount/cache and graph teardown; `make test-umount` |
| 56 | Kernel process accounting | Done | Git history; [V7 accounting/native record contract](accounting.md); acct(51), fork/exec/exit flags, compressed times, real IDs/TTY, held-inode lifetime and complete-record EOF publication; `make test-accounting` |
| 57 | ptrace stop/continue and process access | Done | Git history; [V7 tracing/native register contract](ptrace.md); parent-only access, stop/wait/continue, native register/FPU reads and edits, signal selection and traced exec; `make test-ptrace` |
| 58 | ptrace single-step and debugger memory changes | Done | Git history; [V7 debugger writes and stepping](debugger.md); private instruction/data patches, text exclusion, RX/NX protection, INT3, TF and syscall/fork/signal/exec stepping; `make test-debugger` |
| 59 | profil sampling | Done | Git history; [V7 profil/native sampling](profiling.md); syscall 44, V7 buckets, 16-bit counter wrap, user/system tick charging, safe buffer faults and fork/exec lifetime; `make test-profile` |
| 60 | mcount, monitor and profiling startup objects | Done | Git history; [Native monitor/mcount and startup](monitor.md); bounded call records, ABI-transparent instrumentation, mcrt0/fmcrt0, exit flushing and versioned mon.out; `make test-monitor` |
| 61 | ELF nlist and executable symbol interfaces | Done | ELF64 nlist, shared symbol/header contracts, validated table/name bounds; host and native QEMU acceptance; [contract](nlist.md); Git history |
| 62 | Kernel-information snapshots | Done | Git history; [versioned native snapshots](kernel-information.md); process/file/inode/text/mount/memory/I/O, root access, coherent pointer-free records and live metadata; `make test-kinfo` |
| 63 | Resident-memory lock interface | Done | Git history; [V7 lock and native residency](resident-lock.md); syscall 53, effective-root checks, SULOCK and fork/exec/exit lifetime; `make test-lock` |
| 64 | Privileged phys interface | Done | Git history; [native physical windows](physical-mappings.md); syscall 52, owned RAM/VGA, RW/NX aliases, atomic rollback and fork/exec/reference cleanup; `make test-phys` |
| 65 | Multiplexed group/channel creation and ownership | Done | Git history; `make test-mpx`; native syscall 56; bounded groups/channels, tree indices, join/extract/detach and fork/exec/close ownership; [contract](multiplexor.md); data/control added by 66; TTY/libc added by 67 |
| 66 | Multiplexed data/control events and flow control | Done | Git history; `make test-mpx-data`; native record I/O, bounded queues, EOF/close/open events, listener/attach handshake, high/low-water flow control, interruptible wakeups and pinned detach lifetime; [contract](multiplexor.md); TTY/libc added by 67 |
| 67 | Multiplexed TTY bridging and libc mpx wrappers | Done | Git history; `make test-mpx-tty`; retained TTY ports, directional connect/disconnect, cooked/raw data, signal/flow events, process groups, ioctl request/reply and twelve libc exports; [contract](multiplexor.md); independent TTY hardware remains 89–93 |
| 68 | V7 serial packet line discipline | Done | [V7 serial packet contract](serial-packets.md); `make test-packet` at 16/128 MiB, host protocol/ownership/UART/libc tests; checksums, negotiation, windows/retries, EOF, mpx ports and last-close cleanup; Git history |
| 69 | Native memory/special-device interfaces | Done | [V7 memory/special-device contract](memory-devices.md); `make test-memdev`; privileged mem/kmem byte access, backing/page permissions, null aliases, controlling-TTY forwarding and shared offsets; host and 16/128 MiB acceptance; Git history |
| 70 | Historical disk-controller equivalents | Done | [Native V7 disk families](disk-controllers.md); `make test-disk`; five geometry/unit/partition contracts, bounded recovery, ATA backing, mounted-alias protection and cold-boot persistence at 16/128 MiB; host failure/lifetime tests; Git history |
| 71 | Sequential tape-device equivalent | Done | [HT/TM tape contracts](tape-devices.md); `make test-tape`; regular-file media, raw/buffered records, marks/spacing/rewind, exclusive ownership, bounded recovery and cold-boot persistence at 16/128 MiB; native controls; Git history |
| 72 | DECtape-device equivalent | Done | [TC contracts and format](dectape.md); `make test-dectape`; eight 578-block units, random/partial I/O, bounded retries, persistent select errors, file-backed media, mounted-alias guards, tp-format and nested V7 filesystem fixtures; cold boots at 16/128 MiB; Git history |
| 73 | Disk ordering/interleave helpers | Done | [V7 queue/interleave contracts](disk-ordering.md); `make test-disk-order`; source-differential helpers, real HP request dispatch, one/two-drive block interleave, member lifetime/read-only/mount guards and striped V7 filesystem persistence at 16/128 MiB; Git history |
| 74 | Historical terminal-controller equivalents | Done | DC/DH/DZ/KL profiles, parity/errors, TANDEM, LCASE and timed output; `make test-tty-controllers`; [contracts and limits](tty-controllers.md); Git history |
| 75 | Modem and dial-control equivalents | Done | [Native modem/dial contracts](modems.md); local/UART/virtual carrier, SIGHUP, B0, DN dialing and DU synchronous records; `make test-modem`; forked peers at 16/128 MiB, host handshake/error/timeout tests; Git history |
| 76 | Typesetter device equivalent | Done | C/A/T exact binary capture endpoint with exclusive open, pinned backing/lifetime and checked storage errors; make test-typesetter test-output-unit; docs/output-devices.md; Git history |
| 77 | Printer/plotter device equivalents | Done | VP mode/control framing and PPLOT transition; VS GO burst/input protocols with bounded peer; make test-printer test-typesetter test-output-unit; docs/output-devices.md; Git history |
| 78 | Native kernel configuration/build tooling | Done | Validated native configuration generates actual kernel driver link inputs; native build recipes and every historical configuration responsibility mapped; make test-mkconf build test-boot test-multiuser-acceptance; Git history |
| 79 | Resolve V6 wrappers and PDP-11 runtime assembly replacements | Done | Every V6 wrapper and libc compiler helper mapped to native ABI; signed/compound arithmetic, stack/varargs and API guest tests at 16/128 MiB; docs/runtime-equivalents.md; Git history |
| 80 | Complete native libm coverage and source-gap resolution | Done | All retained math families plus pinned historical gamma/signgam equivalent; safe large trig/Bessel boundaries and IEEE cases; make test-math-coverage test-libc test-libc-interfaces; docs/math-coverage.md; Git history |
| 81 | Document/test native replacement for libfpsim/fakfp | Done | Native x87/SSE2 mapping for every libfpsim/fakfp input; arithmetic/conversion/SIGILL guest acceptance; make test-fpsim-equivalent test-fpu; docs/fpsim-equivalent.md; Git history |
| 82 | Final libc consumer acceptance and installed development interfaces | Done | Installed native headers/libc/libm and all four crt variants; 174-input libc responsibility audit, V7 execvp script/retry closure and staged-only consumer builds at 16/128 MiB; docs/development-interfaces.md; Git history |

### Source inputs and independent terminals

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 83 | Manifest-driven build/install staging | Done | [Native installation manifest and staging](install-staging.md); shared build/catalog/root plan, modes/UID/GID, verified payload hashes and safe stage replacement; `make test-install`; Git history |
| 84 | Scalable V7 image builder | Done | [Image sizing and limits](image-builder.md); automatic volume/inode sizing, free-space budgets, mapped large images and atomic output; 64 MiB/4096-inode host and 16/128 MiB QEMU acceptance; `make test-image-builder`; Git history |
| 85 | Separate full IDE root from embedded bootstrap/test root | Done | [Root image profiles](root-images.md); fixed bootstrap manifest/config and 512 KiB RAM image, independent full IDE seed and atomic non-clobber disk creation; kernel hash isolation and cold boots at 16/128 MiB; `make test-root-separation`; Git history |
| 86 | Per-component guest acceptance harness | Done | [Guest acceptance](guest-acceptance.md); component suites, disposable staged IDE roots, serial fixtures, output/status assertions and JSON/log reports; `make test-guest-harness`; Git history |
| 87 | Historical source/data archive extraction | Done | Validated PDP-11 ar reader/extractor; all twelve source/lesson archives round-trip with per-member provenance; make test-v7-archives; docs/source-archives.md; Git history |
| 88 | Resolve missing referenced support inputs | Done | bc math and later Ratfor sources separately pinned/licensed; tested native extended-byte lex equivalent retains optional mode; input provenance/COMMON and all-byte sanitizer fixtures; make test-support-inputs; docs/support-inputs.md; Git history |
| 89 | Separate per-terminal queues, settings and ownership | Done | Independent TTY queues/settings/ownership with bound transports; `make test-tty-lines-unit`; COM1 controller/modem/mpx regressions and QEMU TTY/packet acceptance; Git history |
| 90 | COM2 hardware initialization and interrupt routing | Done | Detected COM2 16550 transport at 0x2f8, IRQ3/vector 35, independent TX ring, RX errors and modem routing; `make test-serial-secondary` at 16/128 MiB; COM1 TTY/rootfs/packet regressions; Git history |
| 91 | Independent controlling terminals and per-TTY signals | Done | Per-line controlling terminals/groups and V7 fork/exec/close lifecycle validated on both UARTs; `make test-terminal-sessions` at 16/128 MiB; cooked editing, empty lines, EOF, SIGINT and XOFF/XON isolation; [contract](multi-terminal.md); Git history |
| 92 | Package ttyS1 and terminal configuration | Done | Full-root ttyS1 node and disabled V7 ttys records; default COM2 pipe and `make terminal` connector; `make test-terminal-config` at 16/128 MiB, staging and bootstrap separation checks; [usage](multi-terminal.md); Git history |
| 93 | Two-terminal end-to-end acceptance | Done | Concurrent native V7 shells, partial input, ctty writes, foreground Ctrl-C, empty lines, EOF and four restarts; exact process/file/inode/text/free-page recovery after each pair; `make test-two-terminals` at 16/128 MiB; Git history |

### Multi-user startup and accounts

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 94 | Install native passwd/group/ttys/rc configuration | Done | Native passwd/group/ttys/rc/motd, locked service identities, development DES passwords and private owned homes/profiles; `make test-account-config` at 16/128 MiB; bootstrap unchanged; [accounts](multi-user.md); Git history |
| 95 | Port stty | Done | Retained stty source with native 115200 reporting and bounded operand/error handling; installed command tests on both terminals, independent settings, raw/echo, malformed input and non-TTY descriptors; `make test-stty` at 16/128 MiB; Git history |
| 96 | Port tty | Done | Retained tty command installed with fd-0 identity and original -s behavior; COM1/COM2, piped output, regular-file and pipe input checked with exact status/output; `make test-tty-command` at 16/128 MiB; Git history |
| 97 | Port mesg | Done | Retained mesg with V7 mode/status behavior; per-line permission changes, non-TTY/usage errors and real non-owner chmod denial; `make test-mesg` at 16/128 MiB; Git history |
| 98 | Port getty | Done | Retained getty tables/editing/exec with native 115200 profile, Ctrl-C, clean ASCII and bounded name terminator; private login handoff, EOT, editing/case/length and speed-cycle acceptance on both UARTs; `make test-getty` at 16/128 MiB; Git history |
| 99 | Port login authentication and credentials | Done | Retained login with DES retries, checked credential drop, home, environment, motd and shell exec; native execlp consumer gap closed; hidden-password, UID/GID/cwd/umask, user shell and EOF acceptance; `make test-login` at 16/128 MiB; Git history |
| 100 | Integrate login/logout utmp and wtmp | Done | Shared native V7 login/rmut record adapters; one-based utmp, O_APPEND wtmp, RTC record times and per-line logout; two authenticated sessions with peer preservation and getty respawn; `make test-session-accounting` at 16/128 MiB; Git history |
| 101 | Complete multi-user init startup and respawn | Done | Automatic full-disk getty/login on both lines; source init rc/respawn/reload, checked line failures, distinct groups, group cleanup, preserved accounting and single-user recovery; `make test-multiuser-init` at 16/128 MiB; explicit single-user regression fixtures; Git history |
| 102 | Port passwd | Done | Retained set-ID passwd, real-UID authorization, old/new password prompts and historical status; exclusive lock, recoverable V7 link publication and checked I/O; fault injection and changed-password login/mode acceptance; `make test-passwd-command` at 16/128 MiB; Git history |
| 103 | Port su | Done | Retained set-ID su with original environment/cwd preservation and target shell; checked credentials, hidden/wrong passwords, root bypass, real/effective IDs, nested sessions and failed shell; `make test-su` at 16/128 MiB; Git history |
| 104 | Port newgrp | Done | Retained set-ID newgrp membership/other/password rules and replacement shell; checked UID drop, all native extra descriptors closed, shared V7 /tmp prerequisite; `make test-newgrp` at 16/128 MiB including real group passwords; Git history |
| 105 | Port who | Done | Retained who modes and native utmp formatting; per-line identity, no-TTY fallback, explicit records, eight-byte nonterminated fields, missing files and logout; `make test-who` at 16/128 MiB; Git history |
| 106 | Port write | Done | Retained write messaging, explicit tty, mesg permissions, headers/bells, local shell escape and EOF/SIGINT; bounded fields/paths/buffers, typed signals and pre-open device checks; `make test-write` at 16/128 MiB; Git history |
| 107 | Port wall | Done | Retained asynchronous per-user wall broadcast; checked 3000-byte bound, streams and fixed-width device paths; file/stdin delivery, non-root/root mesg behavior and preserved regular-file trap; `make test-wall` at 16/128 MiB; Git history |
| 108 | Multi-user boot and reboot acceptance | Done | Exact production-seed two-user boot and cold reboot; hidden authentication, permissions, messaging, passwd/su/newgrp owner metadata, RTC/persistent times and files; non-clobber reseeding and four-login exact resource recovery and legacy getty parity handoff; `make test-multiuser` at 16/128 MiB; working disk preserved; Git history |

### Additional libraries

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 109 | Port libmp arithmetic and representation | To do | Native widths; independent multi-precision fixtures |
| 110 | Complete libmp I/O and consuming-program acceptance | To do | Round trips and xsend/dc consumers |
| 111 | Port libdbm | To do | Native archive, persistent database and collision fixtures |
| 112 | Port libF77 scalar/math intrinsics | To do | Integer, real and double interfaces |
| 113 | Port libF77 character/complex intrinsics | To do | Hidden lengths, complex values and ABI |
| 114 | Port libF77 process/error/runtime services | To do | Exit, signals, command-line and Fortran startup |
| 115 | Port libI77 formatted/sequential I/O | To do | Format parsing and read/write/error fixtures |
| 116 | Port libI77 unformatted I/O | To do | Explicit native records and historical-data adapters |
| 117 | Port libI77 direct/list/namelist I/O | To do | Positioning, inquiry and type fixtures |
| 118 | Complete Fortran runtime archive integration | To do | Actual native compiler consumers after backend tasks |

### Essential commands and scripts

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 119 | Port basename | Done | Retained source; native build/install, error/status and guest fixtures; `make test-basename` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 120 | Port cal | Done | Retained source; native build/install, error/status and guest fixtures; `make test-cal` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 121 | Port chgrp | Done | Retained source; native build/install, error/status and guest fixtures; `make test-chgrp` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 122 | Port chmod | Done | Retained source; native build/install, error/status and guest fixtures; `make test-chmod` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 123 | Port chown | Done | Retained source; native build/install, error/status and guest fixtures; `make test-chown` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 124 | Port expr | Done | Retained source; native build/install, error/status and guest fixtures; `make test-expr` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 125 | Port file | Done | Retained source; native build/install, error/status and guest fixtures; `make test-file` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 126 | Port kill | Done | Retained source; native build/install, error/status and guest fixtures; `make test-kill` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 127 | Port mkdir | Done | Retained source; native build/install, error/status and guest fixtures; `make test-mkdir` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 128 | Port mv | Done | Retained source; native build/install, error/status and guest fixtures; `make test-mv` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 129 | Port nice | Done | Retained source; native build/install, error/status and guest fixtures; `make test-nice` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 130 | Port osh | Done | Retained source; native build/install, error/status and guest fixtures; `make test-osh` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 131 | Port rmdir | Done | Retained source; native build/install, error/status and guest fixtures; `make test-rmdir` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 132 | Port sleep | Done | Retained source; native build/install, error/status and guest fixtures; `make test-sleep` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 133 | Port sync | Done | Retained source; native build/install, error/status and guest fixtures; `make test-sync` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 134 | Port test | Done | Retained source; native build/install, error/status and guest fixtures; `make test-test` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 135 | Port yes | Done | Retained source; native build/install, error/status and guest fixtures; `make test-yes` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 136 | Install/test false script | Done | Retained source; native build/install, error/status and guest fixtures; `make test-false` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 137 | Install/test true script | Done | Retained source; native build/install, error/status and guest fixtures; `make test-true` at 16/128 MiB; [contract](essential-commands.md); Git history |
| 138 | Install/test nohup script | Done | Retained source; native build/install, error/status and guest fixtures; `make test-nohup` at 16/128 MiB; [contract](essential-commands.md); Git history |

### Text processing and editing

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 139 | Port cmp | Done | Git history; [native command contract](text-commands.md); retained binary comparison and V7 output, checked skip counts and native read errors; ASan/UBSan plus binary/pipe/status acceptance at 16/128 MiB |
| 140 | Port col | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained page buffer, line motions and overstrikes with growing 1 MiB-bounded lines; 1,600 reference-algorithm comparisons, ASan/UBSan and 16/128 MiB guests |
| 141 | Port comm | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); writable column prefixes, unbounded/unterminated lines, flags/stdin/errors; ASan/UBSan and 16/128 MiB guests |
| 142 | Port diffh | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained resynchronizing comparison with growing lines, rotated buffers and diff-compatible statuses; 300 reference comparisons, ASan/UBSan and 16/128 MiB guests via diff -h |
| 143 | Port diff | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained Stone LCS and normal/-e/-f/-b output without use-after-free reallocation, defined shellsort, distinct EOF and unterminated-line handling; 1,600 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 144 | Port diff3 | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); original script plus /usr/lib/diff3 helper with growing tables and lines, fixed duplicate() and sentinel; 1,200 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 145 | Port ed | Done | Git history; [editor contract](text-commands.md#ed-task-145); retained command/BRE/rotor logic with LP64 line-table fixes, bounded input and exclusive temp creation; ASan/UBSan, edit/save/reopen, growth, encrypted-file oracle, SIGINT/SIGHUP, shell escapes and permissions at 16/128 MiB |
| 146 | Port grep | Done | Git history; [native command contract](text-commands.md); retained V7 flags and BRE bytecode, bounded compiler/matcher, LP64 counts; ASan/UBSan and installed files/pipes/status acceptance at 16/128 MiB |
| 147 | Port egrep | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained grammar through native V7 yacc, 256-byte rows, bounded states/positions, separate -f stream and unbounded lines; 1,200 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 148 | Port fgrep | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained keyword automaton, 20000-state lists, NUL-safe transitions, unbounded lines and V7 block numbers; 2,100 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 149 | Port find | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained V7 traversal and expressions with typed operand nodes, bounded paths/nodes/-exec arguments, root and trailing-slash starts, V7 cpio output; host V7-directory shim, ASan/UBSan and 16/128 MiB guests |
| 150 | Port join | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained merge/rewind, growing lines and fields, empty missing fields, checked -j/-o/-e; ASan/UBSan and 16/128 MiB guests |
| 151 | Port look | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); binary-search boundaries, -d/-f/-t and unbounded entries; dictionary installation remains task 280; ASan/UBSan and 16/128 MiB guests |
| 152 | Port od | Done | Git history; [native command contract](text-commands.md); retained V7 formats and offsets with explicit 16-bit little-endian words, bounded native arithmetic and partial-row handling; independent byte oracle, ASan/UBSan and 16/128 MiB guests |
| 153 | Port pr | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained pagination, columns, merge and terminal-mode handling; single close, checked columns, 64 KiB column pages with overrun detection; 1,650 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 154 | Port rev | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); unbounded and unterminated lines, files/pipes and errors; ASan/UBSan and 16/128 MiB guests |
| 155 | Port sed | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained two-file editor with explicit command union, 8192-byte bounded buffers, 8-bit classes/y/replacements, terminated parsing and empty-match progress; 1,200 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 156 | Port sort | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); retained keys, -c/-m/-u/-o and temporary runs with a 512 KiB native workspace, carried oversize lines, growing merge lines, exclusive temporaries and -u merge fix; 6 MB multi-level merges, 300 reference comparisons, ASan/UBSan and 16/128 MiB guests |
| 157 | Port sp | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); tab packing, widths and unbounded lines; ASan/UBSan and 16/128 MiB guests |
| 158 | Port split | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); checked counts, 676-piece and name limits, byte reconstruction; ASan/UBSan and 16/128 MiB guests |
| 159 | Port tabs | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); exact sequences for every terminal type, native gtty/stty delays and no-terminal detection; ASan/UBSan and 16/128 MiB guests |
| 160 | Port tail | Done | Git history; [native command contract](text-commands.md); retained V7 line/byte/block/reverse paths, safe native argument and I/O handling; ASan/UBSan ring-boundary fixtures and files/pipes at 16/128 MiB |
| 161 | Port tee | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); unlimited outputs, append, interrupt handling and open/write failures; ASan/UBSan and 16/128 MiB guests |
| 162 | Port tr | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); ranges, octal, -c/-d/-s, NUL deletion, all byte values and literal-safe parsing; ASan/UBSan and 16/128 MiB guests |
| 163 | Port tsort | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); ordering, cycle reports matching V7, odd data and unbounded names; ASan/UBSan and 16/128 MiB guests |
| 164 | Port uniq | Done | Git history; [native command contract](text-commands.md#retained-text-utilities-tasks-140164); -u/-d/-c, field/character skips, growing lines and output files; ASan/UBSan and 16/128 MiB guests |
| 165 | Port wc | Done | Git history; [native command contract](text-commands.md); retained V7 word classification, option order and totals; native read/open/write errors, ASan/UBSan and 16/128 MiB installed command acceptance |

### Build generators and awk

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 166 | Port cb | To do | C-source formatting fixtures |
| 167 | Port m4 | To do | Macros, quoting, includes and recursion |
| 168 | Port make | Done | Git history; [native make contract](make.md); retained V7 dependency engine, bounded native grammar parser and ar/ELF adapters; timestamps, suffix/wildcard rules, macros, signals, permissions and failure flags; ASan/UBSan and 16/128 MiB guests |
| 169 | Port yacc grammar processing and tables | Done | Git history; [native LALR generator](yacc.md); make test-yacc test-inventory test-install-unit; production sanitizer, typed/precedence parser execution, 80-terminal lookaheads and exact native C/header/report output at 16/128 MiB |
| 170 | Complete yacc skeleton and native parser generation | Done | Git history; [native skeleton](yacc.md); make test-yacc test-inventory test-install-unit; generated pointer semantics, empty rules, error recovery and overflow under ASan/UBSan, exact native generation and parser execution at 16/128 MiB |
| 171 | Port lex scanner generation | Done | Git history; [native lex](lex.md); `make test-lex test-inventory test-install-unit`; production generator under ASan/UBSan, native ncform driver, C/uncompressed/%T scanner execution, Ratfor generation, checked limits and malformed specifications; exact guest output at 16/128 MiB as root/user |
| 172 | Port libln and lex/smallex runtime targets | Done | Git history; [libln and smallex](lex.md#libln-and-smallex); `make test-lex test-inventory test-install-unit`; installed /lib/libln.a with REJECT/yyless/allprint/main/yywrap under ASan/UBSan, SMALL-limit generator; prebuilt and native `ld -lln` scanners at 16/128 MiB |
| 173 | Complete lex/yacc native generator acceptance | Done | Git history; [native consumers](lex.md#native-consumers); `make test-lexyacc test-c-compiler test-as test-libc test-inventory test-install-unit`; installed cc compiles compressed/%T/libln scanners and a yacc+lex calculator at 16/128 MiB as root/user, uncompressed at 128 MiB; c1 LP64 address-constant initializers; leaner as records and in-place realloc; Ratfor execution moved to task 216 |
| 174 | Port awk parser and generated helper tables | Done | Git history; [awk front end](awk.md); `make test-awk-parse test-lex test-lexyacc test-inventory test-install-unit`; native yacc/lex/tokenscript/proc generation, LP64 parser values and nodes, bounded strings, regex positions/states/bytes and lex tokens, single-owner follow sets; hand-checked trees/DFAs, diagnostics and 400 random programs under ASan/UBSan; guest regeneration with yacc/lex/ed/proc and native trees at 16/128 MiB as root/user |
| 175 | Port awk interpreter and data model | Done | Git history; [awk interpreter](awk.md#interpreter-and-data-model-task-175); `make test-awk-run test-awk-parse test-inventory test-install-unit`; native-width numbers/conversions, safe OFMT, separate string constants, EOF records, null fields, bounded records/print/substr/split, temporary and array lifetimes; 40 exact cases (19 against mawk) and 300 random programs under ASan/UBSan; native awk at 16/128 MiB as root/user |
| 176 | Complete awk I/O/functions and end-to-end acceptance | Done | Git history; [awk I/O and acceptance](awk.md#io-formatting-and-acceptance-task-176); `make test-awk-run test-awk-parse test-inventory test-install-unit`; installed /bin/awk; bounded per-call printf/sprintf with typed conversions, literal unknown items, redirection/file/pipe closing, grammar error recovery, deferred array freeing; 58 exact cases (25 against mawk, 42×28 regex matrix), complete programs and random printf formats under ASan/UBSan; 16/128 MiB as root/user |

### Native C/Fortran toolchains

The target is ELF64 AMD64, SysV calling conventions, LP64 and static linking.
Host tools may bootstrap generated code; both supplied C compiler families
are accounted for. Final native rebuild acceptance is task 207.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 177 | Freeze native object/toolchain ABI | Done | Git history; [version 1 contract](toolchain-abi.md), installed ident.h; make test-toolchain-abi test-development test-inventory; LP64/ELF/relocations, independent register/stack/AL inspector, mixed/large aggregates, varargs, long-double and four startup variants at 16/128 MiB |
| 178 | Port cpp | Done | Git history; native V7 scanner, LP64 lazy expressions, includes and generated-source fixtures; `make test-cpp` at 16/128 MiB; [port notes](cpp.md) |
| 179 | Port ar | Done | Git history; [native archive commands and formats](archives.md); make test-ar test-inventory test-install-unit; sanitizer, GNU linker/execution, failure preservation and 16/128 MiB guest fixtures |
| 180 | Port arcv | Done | Git history; [historical conversion](archives.md); make test-arcv test-inventory test-install-unit; V6/V7 metadata/payload preservation, malformed-input rejection, installed nm consumption at 16/128 MiB |
| 181 | Port assembler parser, symbols and directives | Done | Git history; [native first-pass model](assembler.md); make test-as-parse test-inventory test-install-unit; GNU syntax oracle, sanitizer, operands/expressions/symbols/directives and private native consumer at 16/128 MiB; encoding completed in 182; object output completed in 183; pipeline remains 184 |
| 182 | Implement x86_64 instruction encoding | Done | Git history; [native integer/SSE2/x87 encoder](assembler.md); make test-as-encode test-as-parse test-inventory test-install-unit; production sanitizers, GNU byte/relocation oracle, randomized addressing and encoded System V functions at 16/128 MiB |
| 183 | Implement assembler ELF relocatable output | Done | Git history; [ELF producer](assembler.md); make test-as-object test-as-encode test-as-parse test-inventory test-install-unit; sanitizer, independent section/relocation inspection and linked execution, exact native production and nm/size/ar/ranlib consumption at 16/128 MiB |
| 184 | Complete assembler pass/helper integration | Done | Git history; installed /bin/as and /lib/as2; make test-as test-as-object test-as-encode test-as-parse test-inventory test-install-unit; sanitizer CLI tests, multi-file/stdin, native exact ELF bytes and linked execution at 16/128 MiB |
| 185 | Port linker symbol/archive resolution | Done | Git history; [native resolution pass](linker.md); `make test-ld-resolve test-inventory`; GNU ld/nm oracle, sanitizer, archive search/order, strong/weak/common and error fixtures; private native consumers at 16/128 MiB; output/install remain 186–187 |
| 186 | Implement linker relocations and executable layout | Done | Git history; [ELF64 executable pass](linker.md); `make test-ld-output test-ld-resolve test-inventory`; GNU relocation oracle, host execution, sanitizer/error fixtures and native C/crt0/libc linking/execution at 16/128 MiB; installed command remains 187 |
| 187 | Complete native linker acceptance | Done | Git history; installed `/bin/ld`; [command/native adaptations](linker.md); `make test-ld test-ar-unit test-strip-unit test-install-unit test-inventory`; sanitizer/GNU -r interoperability, native as/ar pipeline, C/libc execution and linker self-link at 16/128 MiB |
| 188 | Port nm | Done | Git history; [native ELF/archive modes](object-tools.md); make test-nm test-inventory; ASan/UBSan and 16/128 MiB installed selection/sort/prefix, long-name, GNU/BSD/V7-container and malformed-input fixtures |
| 189 | Port size | Done | Git history; [native object sizes](object-tools.md); make test-size test-nm test-inventory; GNU size oracle, archive long names, sectionless executable segments, default a.out and malformed input under ASan/UBSan and 16/128 MiB guests |
| 190 | Port strip | Done | Git history; [native ELF/archive stripping](archives.md); make test-strip test-ar test-arcv test-ranlib test-inventory test-install-unit; sanitizer/mutated ELF, ELF/PIE execution, preservation/idempotence and native execution at 16/128 MiB |
| 191 | Port ranlib | Done | Git history; [native ELF archive indexes](archives.md); make test-ranlib test-inventory test-install-unit; independent big-endian index/offset checks, linked C execution, malformed ELF preservation, 16/128 MiB installed acceptance |
| 192 | Retarget shared mip compiler infrastructure | Done | Git history; [native shared services and consumer ownership](mip.md); `make test-mip test-mip-infra test-mip-pass test-pcc-front test-pcc-integer test-inventory`; sanitizer/independent stream oracle, real direct/split PCC compilation, alternate producer and independent analysis consumption at 16/128 MiB as root/user; full compiler/lint/Fortran commands retain separate tasks |
| 193 | Retarget PCC front end | Done | Git history; [retained frontend and native header profile](pcc.md); `make test-pcc-front test-pcc-integer test-mip test-mip-infra test-mip-pass test-inventory`; typed/K&R signatures, native extended types/literals, all 49 libc headers, safe failures and scalar execution/oracles at 16/128 MiB as root/user; remaining machine ABI/driver/rebuild work is 195/196/207 |
| 194 | PCC integer/pointer/control-flow backend | Done | Git history; [native integer backend](pcc.md); `make test-pcc-integer test-pcc-front test-mip-infra test-inventory`; sanitizer/GCC execution oracle, GP/stack ABI, exact native assembly and cpp/PCC/as/ld execution as root/user at 16/128 MiB; calls/floating/aggregate remain 195 |
| 195 | PCC calls, structures and floating-point backend | Done | Git history; [aggregate/x87/variadic ABI](pcc.md); `make test-pcc-abi test-pcc-integer test-pcc-front test-mip test-mip-infra test-mip-pass test-inventory`; System V aggregate classes, structure arguments/results, x87 long double, variadic definitions/va_copy against GCC callers/callees/oracle; exact native cpp/PCC/as/ld execution as root/user at 16/128 MiB |
| 196 | Complete PCC driver and supplied alternate targets | Done | Git history; [installed ccom, pcc and fort](pcc.md); `make test-pcc-driver test-pcc-integer test-pcc-abi test-pcc-front test-cpp test-inventory test-install-unit`; `/usr/lib/ccom` options, `/bin/pcc` with native profile, -O/-X/-S/-p/-c, fort with independent producer; cpp arena grows on demand; 16/128 MiB as root/user |
| 197 | Retarget original C c0 front end | Done | Git history; [native c0](c-compiler.md); `make test-c-compiler test-inventory test-install-unit`; LP64 widths/layout, 64-bit pass format, exact identifiers, V7 dialect and native header profile (all 49 libc headers); sanitizer, mutation and 90 supplied command sources; installed `/lib/c0` at 16/128 MiB as root/user |
| 198 | Retarget original C c1 backend | Done | Git history; [table-driven AMD64 c1](c-compiler.md); `make test-c-compiler test-inventory test-install-unit`; cvopt-converted AMD64 table, integer/pointer/control/call/switch generation; GCC execution oracle through GNU and native as; native cpp/c0/c1/as/ld fixture and V7 commands as root/user at 16/128 MiB |
| 199 | Retarget original C floating/aggregate generation | Done | Git history; [floating and structure ABI](c-compiler.md); `make test-c-compiler test-inventory test-install-unit`; SSE floating values/conversions/unordered branches, System V structure classes, arguments/results/hidden pointers, prototype float parameters, variadic definitions; GCC oracle and GCC caller/callee fixtures; native execution as root/user at 16/128 MiB |
| 200 | Retarget c2 optimizer and conversion helpers | Done | Git history; [AMD64 c2, fc1, cvopt](c-compiler.md); `make test-c-compiler test-inventory test-install-unit`; supplied jump-graph optimizations on AT&T assembly, conservative register tracking; optimized/unoptimized execution equivalence for fixtures, ABI and 90 V7 sources; fc1 = c1; native cvopt reproduces the table; 16/128 MiB as root/user |
| 201 | Port cc driver | Done | Git history; [installed cc](c-compiler.md); `make test-cc test-inventory test-install-unit`; supplied driver over cpp/c0/c1/fc1/c2/as/ld with mcrt0/fcrt0, -c/-S/-O/-P/-E/-f/-p/-o/-D/-I/-t/-B, failures; 16/128 MiB as root/user |
| 202 | Install/test lorder script | Done | Git history; [adapted script/native join](object-tools.md); make test-lorder test-install-unit test-inventory; definition/reference graphs including common/read-only symbols and cycles, original single-object self-pairs, long archive names and errors under ASan/UBSan and 16/128 MiB guests |
| 203 | Retarget f77 parser and semantic analysis | To do | f77pass1 and native symbol/type sizes |
| 204 | Retarget f77 integer/control/array backend | To do | Native frame, array and COMMON/EQUIVALENCE layouts |
| 205 | Retarget f77 real/complex/call backend | To do | Fortran ABI, hidden lengths and runtime calls |
| 206 | Complete f77 driver and runtime startup integration | To do | Compiled numerical and I/O programs |
| 207 | Native toolchain rebuild acceptance | To do | Rebuild supported V7 tools/userland inside Nexix; modern kernel/libc use host GCC |

### Mathematics and language tools

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 208 | Port dc | To do | Numeric/language fixtures and native execution |
| 209 | Port bc | To do | dc integration; install/test all six functions from the separately recovered lib.b input |
| 210 | Port factor | To do | Numeric/language fixtures and native execution |
| 211 | Port primes | To do | Numeric/language fixtures and native execution |
| 212 | Port random | To do | Numeric/language fixtures and native execution |
| 213 | Port spline | To do | Numeric/language fixtures and native execution |
| 214 | Port units | To do | Install units data and numeric conversion fixtures |
| 215 | Port bas | To do | Supplied build is obsolete; restore required old library interfaces |
| 216 | Port ratfor | To do | Numeric/language fixtures and native execution; also run `lex -r` scanners (from task 173) |
| 217 | Port structure | To do | Retained restructuring engine |
| 218 | Port beautify and struct script | To do | libln dependency and complete wrapper behavior |

### Storage administration and archives

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 219 | Port mknod | Done | Git history; make test-mknod at 16/128 MiB; decimal bounds, exact b/c modes, root-only creation, umask, real null I/O, duplicate errors and cold-boot node persistence; [contract](storage-commands.md) |
| 220 | Port mount | To do | Native /etc/mtab integration; disposable storage fixtures, bounds, permissions and error paths |
| 221 | Port umount | To do | Shared /etc/mtab lifecycle; disposable storage fixtures, bounds, permissions and error paths |
| 222 | Port mkfs | To do | Format disposable media; mount and populate |
| 223 | Port df | Done | Git history; make test-df at 16/128 MiB; explicit V7 disk encodings, independent free counts, chain/duplicate/cycle/range/read-error fixtures, raw-root access and ordinary-user permissions; [contract](storage-commands.md) |
| 224 | Port du | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 225 | Port dd | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 226 | Port sum | Done | Git history; make test-sum at 16/128 MiB; independent 16-bit oracle, empty/binary/stdin inputs, 512-byte boundaries, multiple files, open/read errors and continuation; [contract](storage-commands.md) |
| 227 | Port clri | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 228 | Port dcheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 229 | Port icheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 230 | Port ncheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 231 | Port quot | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 232 | Port dump | To do | Native /etc/ddate integration, historical archive/media formats and full restore fixtures |
| 233 | Port dumpdir | To do | Historical archive/media formats and full restore fixtures |
| 234 | Port restor | To do | Historical archive/media formats and full restore fixtures |
| 235 | Port tar | To do | Both tar/v6tar; historical header fixtures and native round trips |
| 236 | Port tp | To do | Native tape equivalent and historical directory format |
| 237 | Port update | Done | Git history; make test-update at 16/128 MiB; daemon fork/descriptor handling, real 30-second SIGALRM and persistent V7 superblock clock advancement; [contract](storage-commands.md) |

### Jobs, mail and accounting

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 238 | Install native spool ownership and locking configuration | To do | Safe native fixtures, not historical user/mail contents |
| 239 | Port ac | To do | Source-defined service/accounting fixtures |
| 240 | Port accton | Done | Git history; make test-accton at 16/128 MiB; root enable/disable, error/privilege paths, first-operand compatibility, actual uid-101 process records and stable file after disabling; [contract](accounting.md) |
| 241 | Port sa | To do | Source-defined service/accounting fixtures |
| 242 | Port at | To do | Source-defined service/accounting fixtures |
| 243 | Port atrun | To do | Source-defined service/accounting fixtures |
| 244 | Port cron | To do | Source-defined service/accounting fixtures |
| 245 | Port calendar | To do | Private helper plus installed calendar script |
| 246 | Port makekey | To do | Known historical cipher vectors and pipe interfaces |
| 247 | Port crypt | To do | Known historical cipher vectors and pipe interfaces |
| 248 | Port mail | To do | Local delivery, mailbox locking and permissions |
| 249 | Port xsend cryptographic/message helpers | To do | libmp, native integer representation |
| 250 | Complete enroll/xsend/xget workflows | To do | Two-account enrollment, send and recovery |

### Diagnostics and development

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 251 | Port dmesg | Done | Git history; [kernel log](kernel-log.md); safe root-only native snapshots, timestamped console messages and V7 incremental-save mode; ring/bounds/error tests and VGA/serial acceptance at 16/128 MiB |
| 252 | Port iostat | To do | Native kernel-information interface; no historical structure offsets |
| 253 | Port ps | Done | Native address-free process snapshots; V7 selection/long/child-time modes, errors and ordinary-user tests at 16/128 MiB; `make test-ps`; Git history |
| 254 | Port pstat | To do | Native kernel-information interface; no historical structure offsets |
| 255 | Port time | To do | Diagnostics/status fixtures |
| 256 | Port bs | To do | Diagnostics/status fixtures |
| 257 | Port prof | To do | Native symbols and profiling-record consumers |
| 258 | Port adb ELF/core/symbol reading | To do | Native executable/core interpretation |
| 259 | Port adb x86_64 instruction/register handling | To do | Disassembly and expression/address fixtures |
| 260 | Complete adb live debugging | To do | ptrace, stepping, breakpoints and memory changes |
| 261 | Port lint analysis passes | Done | Git history; [lint1 over the PCC front end, lint2](lint.md); `make test-lint test-inventory test-install-unit`; pass-1 records/warnings, cross-file inconsistencies, VARARGS/ARGSUSED/LINTLIBRARY/NOTREACHED, -h/-p/-v/-x/-u/-a/-b, lint2 self-sorting and malformed input under ASan/UBSan |
| 262 | Complete lint driver and description libraries | Done | Git history; [installed lint](lint.md); `make test-lint test-inventory test-install-unit`; adapted /bin/lint over /usr/lib/pcc.h, native llib-lc/llib-port/llib-lm agreeing with the native headers; exact reports, -n/-p/-l/-D/-I, subdirectories, failures at 16/128 MiB as root/user |

### Documents, manuals and learning

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 263 | Port roff | To do | Retained formatter behavior |
| 264 | Retarget nroff/troff parsing and layout core | Done | Explicit native saved environments and pointer-sized macro frames; source parser/layout retained; `make test-formatter-core` at 16/128 MiB; Git history |
| 265 | Regenerate native terminal/font tables | Done | 12 terminal and 25 font tables regenerated without PDP-11 headers; native mkfont/chars; `make test-formatter-table-helpers` at 16/128 MiB; Git history |
| 266 | Complete nroff terminal output | Done | Installed source-based nroff with portable terminal data; all 301 section 1–8 files render at 16/128 MiB; archived manual header excerpts; `make test-nroff`; Git history |
| 267 | Complete troff typesetter output | Done | Native 224-byte mounted fonts, ASCII output and independently checked CAT framing; font/size/manual fixtures at 16/128 MiB; `make test-troff`; Git history |
| 268 | Port eqn | Done | Original grammar and layout with pointer-sized semantic values, native variadic errors and retained-source host parser generation; `make test-eqn` at 16/128 MiB; Git history |
| 269 | Port neqn | Done | Retained terminal-equation grammar/layout with native semantic pointers and nroff integration; `make test-neqn` at 16/128 MiB; Git history |
| 270 | Port tbl | To do | Tables and formatter integration |
| 271 | Port checkeq | To do | Document-processing fixtures |
| 272 | Port deroff | To do | Document-processing fixtures |
| 273 | Port ptx | To do | Document-processing fixtures |
| 274 | Port refer indexing helpers | To do | mkey/inv/hunt and dictionary/index representations |
| 275 | Complete refer formatting/helpers | To do | refer/deliv/refpart/whatabout/flagger/kaiser/thash and all supplied targets |
| 276 | Install/test lookbib | To do | Original script and native refer helpers |
| 277 | Port prep and supporting header | To do | Native text processing and ownership of top-level prep.h |
| 278 | Port spellin/spellout and hash generation | To do | Regenerate native dictionary hashes |
| 279 | Complete spell command and script | To do | American/British/local/stop inputs and wrapper |
| 280 | Install dictionaries and bibliography inputs | To do | Validate indexes and consuming programs |
| 281 | Install formatter macros/parser descriptions/library data | Done | Original tmac packages, ncform and lint descriptions installed intact; native yaccpar adapted/tested in task 170; native macro rendering at 16/128 MiB; `make test-formatter-macros`; Git history |
| 282 | Install public ASCII/Greek/equation data | Done | All public tables installed unchanged; eqn/neqn character-definition and formatter consumers at 16/128 MiB; `make test-public-data`; Git history |
| 283 | Port learn engine | To do | Sessions, workspace isolation and helper execution |
| 284 | Port learn private tee/lcount helpers | To do | Distinct from /usr/bin/tee |
| 285 | Install/test lesson archives and learn workdirs | To do | All six archives and representative lesson sessions |
| 286 | Install all manual pages and native annotations | Done | All 307 original files installed byte-for-byte; separate native-port notes; `make test-man-pages`; Git history |
| 287 | Install/test original man script | Done | V7 source lookup and formatter modes, section-0 front matter, native notes and navigable root/user VGA/serial paging; timezone-correct date registers; `make test-man` at 16/128 MiB; Git history |
| 288 | Install/test technical documents and driving scripts | To do | Native formatter/tool references |

### Plotting and historical output

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 289 | Extract/adapt generic libplot | To do | Historical .c.a input, native plot records |
| 290 | Port Tektronix libplot backend | To do | libt4014 protocol fixtures |
| 291 | Port t300/t300s libplot backends | To do | Both terminal protocol variants |
| 292 | Port t450/vt0 libplot backends | To do | Archive/source-only targets and native endpoint equivalents |
| 293 | Port plot filters and script | To do | tek/t300/t300s/t450/vplot with wrapper |
| 294 | Port graph | To do | Plot/typesetter/printer protocol capture and consuming fixtures |
| 295 | Port tc | Done | Retained CAT-to-Tektronix converter, bounded prompt input and native initializers; captured protocol/page/scale/error fixtures at 16/128 MiB; `make test-tc`; Git history |
| 296 | Port tk | To do | Plot/typesetter/printer protocol capture and consuming fixtures |
| 297 | Port vpr | To do | Plot/typesetter/printer protocol capture and consuming fixtures |

### Serial communications

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 298 | Port cu | To do | Two native serial endpoints and connection lifecycle |
| 299 | Port UUCP packet/connection and lock primitives | To do | Native serial link and spool ownership |
| 300 | Port uucp/uux submission | To do | Queue creation, paths and authorization |
| 301 | Port uucico transfer daemon | To do | Two-guest serial transfer and retry/disconnect handling |
| 302 | Port uuxqt execution daemon | To do | Remote queue processing and permissions |
| 303 | Port uulog/uuclean/uurecover | To do | Status, cleanup and interrupted-job recovery |
| 304 | Complete UUCP configuration/scripts and acceptance | To do | All supplied targets; serial transport only, no TCP/IP |

### Games with source and scripts

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 305 | Install/test game datasets | To do | Native adapters for fortunes, quiz, hangman and supporting records |
| 306 | Port arithmetic | To do | Scripted terminal/game fixtures and required data |
| 307 | Port backgammon | To do | Scripted terminal/game fixtures and required data |
| 308 | Port fish | To do | Scripted terminal/game fixtures and required data |
| 309 | Port fortune | To do | Scripted terminal/game fixtures and required data |
| 310 | Port hangman | To do | Scripted terminal/game fixtures and required data |
| 311 | Port quiz | To do | Scripted terminal/game fixtures and required data |
| 312 | Port wump | To do | Scripted terminal/game fixtures and required data |
| 313 | Retarget chess engine assembly | To do | Native equivalent of supplied PDP-11 engine |
| 314 | Complete chess command and gameplay acceptance | To do | Legal moves, scripted games and terminal output |
| 315 | Install/test words script | To do | Requires words1 source recovery/native port |
| 316 | Install/test ching script | To do | Requires cno/phx source recovery/native ports |
| 317 | Install/test 1 script | To do | Original script and referenced game behavior |

### Binary-only release blockers

Each program has separate source/provenance recovery and implementation tasks.
Historical binaries remain preserved; recovery or an approved alternative is
required before native-port acceptance.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 318 | Recover banner source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 319 | Port/test banner | To do | Blocked until corresponding source recovery/approved alternative |
| 320 | Recover bcd source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 321 | Port/test bcd | To do | Blocked until corresponding source recovery/approved alternative |
| 322 | Recover bj source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 323 | Port/test bj | To do | Blocked until corresponding source recovery/approved alternative |
| 324 | Recover checkers source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 325 | Port/test checkers | To do | Blocked until corresponding source recovery/approved alternative |
| 326 | Recover cno source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 327 | Port/test cno | To do | Blocked until corresponding source recovery/approved alternative |
| 328 | Recover cubic source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 329 | Port/test cubic | To do | Blocked until corresponding source recovery/approved alternative |
| 330 | Recover maze source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 331 | Port/test maze | To do | Blocked until corresponding source recovery/approved alternative |
| 332 | Recover moo source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 333 | Port/test moo | To do | Blocked until corresponding source recovery/approved alternative |
| 334 | Recover phx source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 335 | Port/test phx | To do | Blocked until corresponding source recovery/approved alternative |
| 336 | Recover ppt source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 337 | Port/test ppt | To do | Blocked until corresponding source recovery/approved alternative |
| 338 | Recover reversi source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 339 | Port/test reversi | To do | Blocked until corresponding source recovery/approved alternative |
| 340 | Recover ttt source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 341 | Port/test ttt | To do | Blocked until corresponding source recovery/approved alternative |
| 342 | Recover words1 source/provenance | To do | Blocked: archive supplies binaries only; alternative needs explicit approval |
| 343 | Port/test words1 | To do | Blocked until corresponding source recovery/approved alternative |

### Native standalone and bootstrap equivalents

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 344 | Native standalone runtime and libsa | To do | srt0, SYS/config/device I/O and isolated runtime |
| 345 | Standalone cat | To do | Boot/runtime file-reading fixture |
| 346 | Standalone mkfs | To do | Disposable-media formatting and native mount verification |
| 347 | Standalone icheck | To do | Known clean/corrupt filesystem fixtures |
| 348 | Standalone restor | To do | Restore into disposable media with standalone runtime |
| 349 | Native boot/mtboot equivalents | To do | Disk/tape startup paths and controlled QEMU acceptance |
| 350 | Port standalone maketape/build helpers | To do | cc+ld-stand/cpgroup and media creation |
| 351 | Account for historical boot/kernel binaries | Done | Git history; [six-image provenance/native-role map](boot-kernel-artifacts.md); make test-boot-artifacts test-inventory; hashes/sizes, exact ownership and negative false-execution/path/omission fixtures; standalone boot remains 349 |

### Rebuild, upgrades and release acceptance

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 352 | Adapt/test in historical build script | To do | Use native paths/tools; all supported component recipes |
| 353 | Adapt/test cmake historical build script | To do | Use native paths/tools; all supported component recipes |
| 354 | Adapt/test makeall historical build script | To do | Use native paths/tools; all supported component recipes |
| 355 | Full native V7 source-tree build acceptance | To do | All supplied source components, helpers, libraries and explicit native equivalents |
| 356 | Package complete 0.1.0 distribution | To do | Programs, scripts, libraries, headers, source/build inputs, manuals/data and permissions |
| 357 | Persistent disk upgrade/install tooling | Done | Git history; [offline upgrade contract](disk-upgrades.md); verified staging, exclusive offline locks, mandatory non-overwritten backup, candidate validation and atomic publication; account/config/TZ/data/hard-link/sparse-file/timestamp preservation; failure tests and two cold boots at 16/128 MiB |
| 358 | Full multi-user/time/storage/userland acceptance | To do | 16/128 MiB baseline; full IDE release at 128 MiB and two terminals; full boot self-test scratch-space exhaustion repaired with a private 1 MiB test volume; 16/128 MiB stress acceptance in `make test-stability`; intermittent freeze remains under investigation ([notes](stability.md)) |
| 359 | Close all component/source/device blockers | To do | All 223 entries/36 requirements traceable; close historical device-directory node/consumer audit; no hidden omissions |
| 360 | Reproducible host-bootstrap build and release provenance | To do | Pin SDK inputs, artifact hashes, licensing and validation evidence |
| 361 | Finalize 0.1.0 release documentation and tag | To do | Only after all required leaves are implemented, tested and committed |

### Additional kernel gaps identified by the source audit

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 362 | Complete V7 CPU usage and scheduler priority decay | Done | Git history; `make test-scheduler-unit test-process`; [policy](kernel-timing.md); usage saturation, per-second decay, nice/wakeup priorities and rotating selection |
| 363 | Complete subsecond ftime clock interface | Done | Git history; `make test-ftime`; atomic syscall 35, 10 ms resolution, V7 timezone fields, rollover, readonly and invalid-user-pointer acceptance at 16/128 MiB |
| 364 | Implement native V7 kernel callout queue | Done | Git history; `make test-callout-unit test-process`; [queue contract](kernel-timing.md); tick deadlines, FIFO ties, bounded capacity, cancellation/rearming and PIT callback/wakeup acceptance; device consumers in tasks 68–77 |

### Additional libc and syscall gaps identified by the reference review

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 365 | Complete V7 sleep alarm and signal compatibility | Done | Git history; `make test-sleep-unit test-sleep`; [contract](kernel-timing.md); atomic deadline/flag wait fixes alarm-before-pause hang; zero/earlier/equal/later alarms, handler restoration, unrelated signals and native elapsed/return-value acceptance at 16/128 MiB |
| 366 | Complete invalid/ignored system-call compatibility | Done | Git history; `make test-syscall-policy-unit test-syscall-policy test-syscall`; [native mapping](syscall-abi.md); V7 nosys EINVAL, nullsys success, native setpgrp exception, assigned-but-unavailable ENOSYS, full-width numbers and process survival |

The [reference review](reference-review.md) records coverage, new tasks
and remaining source-input blockers.

### VGA console and keyboard

The default console now selects exclusive VGA output; serial is an explicit
alternative. The independent three-terminal configuration remains available as
`console=multi`. See [console selection and acceptance](vga-console.md).

Add an independent BIOS VGA/PS/2 login on `/dev/tty0`, preserving COM1,
COM2 and `/dev/console`. UEFI/framebuffer graphics, USB keyboard drivers,
virtual-console switching and scrollback are outside this work.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 367 | Implement BIOS VGA text output | Done | Git history; `make test-vga-unit`; 80×25 display, scrolling, hardware cursor, wrapping and control characters; GRUB text mode and protected kernel mapping; host display tests |
| 368 | Implement PS/2 keyboard input | Done | Git history; `make test-keyboard-unit`; IRQ1, bounded controller initialization, US layout, modifiers and control keys; safely consume release/extended scan codes; host decoding/timeout tests; initialization failure preserves serial access |
| 369 | Integrate independent VGA terminal | Done | Git history; `make test-tty-lines-unit` and 128 MiB QEMU VGA/three-session acceptance; Third TTY, /dev/tty0 (major 1, minor 2), getty/login and session accounting; reuse line discipline and controlling-terminal behavior; preserve COM1, COM2 and /dev/console |
| 370 | Package and document graphical console | Done | Git history; `make test-mkconf test-install-unit`; deterministic gzip round trip and packaged 16 MiB VGA login acceptance; [setup](vga-console.md); Fresh-image device/getty configuration, kernel driver build integration, make run-vga, virt-manager BIOS/standard VGA/PS/2 settings and existing-disk setup instructions; preserve make run and working disks |
| 371 | Validate graphical console management | Done | Git history; `make test-vga` at 16/128 MiB, serial/getty/login/multi-user regressions, libvirt guest and inspected virt-manager console; [evidence](vga-console.md#acceptance-evidence); Automated injected-keyboard and VGA-display login/shell tests at 16/128 MiB; hidden passwords, editing, Ctrl-C/Ctrl-D and getty restart; concurrent serial isolation and terminal identity; affected regressions and manual virt-manager acceptance |

### Execution priorities and release acceptance

VGA console support is complete in tasks 367–371; continue with dependency-ready
remaining kernel, library and userland tasks.

Task 37 records coverage relationships and validates the roadmap. Complete
staging, image and test infrastructure in tasks 83–88 before expanding
installation substantially. Prioritize account/libc and set-ID prerequisites
needed for tasks 89–108, delivering multi-user boot before the long userland tail.
Kernel, compiler and library integration gates run after their required
dependencies, even where those dependencies have higher tracking IDs.

For each native component, test source-defined output/status, important failures,
permissions and actual consuming programs. Binary formats need independent
historical fixtures and explicit encoders/adapters. Keep the reference tree
pristine and record native replacements and unresolved inputs explicitly.

Retain the existing regression suite:

```sh
make TOOLCHAIN_PREFIX= test
```

Add focused acceptance groups for new kernel interfaces, two terminals,
multi-user login, component ports and native toolchains. All mutating storage
tests use disposable images and preserve the working disk. Keep the 16 MiB and
128 MiB baseline checks; accept the full IDE distribution with 128 MiB and
two terminals.

Release acceptance requires multi-user startup, RTC-derived time and persistent
V7 timestamps, complete component coverage, native V7 development tools,
installed manuals/data and resolution of explicit release blockers. All 223
components and 36 requirements must be traceable to tested native implementations,
assets or documented native equivalents. No required feature is silently removed
to declare the release complete.

`make TOOLCHAIN_PREFIX= test-inventory` checks full-archive coverage, byte fidelity
and inventory/roadmap consistency, including numbering, status totals and coverage
of every component, requirement and task. It does not prove native functional acceptance or
completion of the ports.

Version **0.2.0** adds PCI/e1000 and a direct **4.4BSD-Lite2** IPv4/TCP/IP
port, blocking BSD-style sockets and static network configuration. This replaces
the earlier lwIP proposal. The [0.2.0 networking roadmap](networking-roadmap.md)
records implementation milestones, public interfaces and acceptance criteria.
Tasks **412–431** track this future-release work and are included in the overall
status totals; they do not expand the V7 inventory scope. Networking is not yet
implemented.

Start with single-core QEMU i440fx, VGA/PS/2 (or selected serial console), and IDE storage. SMP,
physical hardware, UEFI, IPv6, secure remote access, and complete Nexix self-hosting follow
the first release. See [architecture notes](architecture.md) and
[source provenance](../reference/README.md).

### Native extensions

| Task | Description | Status | Evidence |
| --- | --- | --- | --- |
| 372 | Add native uname command and system identity API | Done | Git history; [uname contract](uname.md); kernel-owned identity, checked native syscall, public libc header, command flags and unprivileged/error acceptance at 16/128 MiB |
| 373 | Add persistent hostname and identity prompts | Done | Git history; [hostname contract](hostname.md); boot configuration, root-only native setter, full domain preservation, hostname command and live credential/hostname prompts; host and 16/128 MiB guest tests |

### Traditional BSD vi extension

Traditional Vi 050325 is an additional 0.1.0 component; see [port contract](vi-port.md).

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 374 | Audit and pin traditional vi sources | Done | Release 050325, full byte/provenance index and audited native dependency/build choices; make test-vi-reference; docs/vi-port.md; Git history |
| 375 | Implement termcap database lookup | Done | Bounded native lookup/inheritance/cancellation/decoding; ASan/UBSan host fixtures and native filesystem guests at 16/128 MiB; make test-termcap test-termcap-guest; Git history |
| 376 | Implement termcap output routines | Done | Native cursor expansion, legacy correction/padding, checked limits and callback failures; sanitizer and 16/128 MiB capture fixtures; make test-termcap-output test-termcap-guest; Git history |
| 377 | Add VGA screen-control sequences | Done | Bounded incremental CSI cursor/erase/standout controls; host guard-cell fixtures, real VGA framebuffer checks and full keyboard/login/session regressions at 16/128 MiB; make test-vga-controls test-vga; Git history |
| 378 | Integrate editor terminal modes and geometry | Done | Checked atomic termios bridge and fixed geometry; physical-terminal TERM defaults; test-termios, test-vga, interfaces and inventory pass at 16/128 MiB; Git history |
| 379 | Port vi library and native ABI dependencies | Done | Native working sources compile/link with checked LP64 interfaces, historical regexp and private I/O; native dependency/startup tests pass at 16/128 MiB; Git history |
| 380 | Port editor buffers and temporary storage | Done | Checked pool/temp allocation and seeks; uid-100 6000-line growth, exclusive collision/0600 ownership, ENOSPC status/cleanup and unchanged saved content pass at 16/128 MiB; Git history |
| 381 | Port ex command execution | Done | Native ex command execution, dumb batch terminal and persistent error status; append/substitute/global/read/write/reopen/shell and error tests pass at 16/128 MiB; Git history |
| 382 | Port interactive vi and view | Done | Native visual vi/view with bounded stack screen buffer; serial and real VGA/PS2 insert/motion/delete/undo/search/save/read-only/restoration pass at 16/128 MiB; Git history |
| 383 | Port preservation and recovery helpers | Done | Native reference helpers validate owners/header bounds and preserve privately; uid-100 SIGHUP, vi -r listing, cold-boot recovery, uid-101 denial and malformed input pass at 16/128 MiB; Git history |
| 384 | Package vi, ex, view and documentation | Done | Installed vi/ex/view/helpers, manuals/licenses, native inventory ownership and terminal defaults; package, installed ex/recovery and backed-up upgrade/two-boot tests pass at 16/128 MiB; Git history |
| 385 | Complete traditional vi acceptance | Done | Combined installed acceptance passes at 16/128 MiB: full-screen VGA/serial, packed/large termcap IDs, interrupt cleanup, concurrent editors, storage/recovery/clear and login/upgrade regressions; docs/vi-acceptance.md; Git history |
| 386 | Add clear terminal command | Done | Installed termcap-driven clear and ANSI/VGA descriptions; test-clear verifies serial bytes, VGA memory and errors at 16/128 MiB; Git history |

### Native monitoring and manual navigation extensions

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 387 | Add uptime and scheduler load averages | Done | Monotonic boot ticks, utmp sessions and real 1/5/15-minute kernel load; `make test-uptime` at 16/128 MiB; Git history |
| 388 | Add navigable manual/text pager | Done | VGA/ANSI pages, lines, arrows, search/repeat, start/end, checked formatter/pager status and interrupt restoration; `make test-pager` at 16/128 MiB; Git history |

### Compatibility closure and reference-review follow-up

Completed initial ports retain their historical evidence. Tasks 389–408 resolve
recorded audit obligations; they do not assert that every command has a defect.
Each closure compares reference behavior and native dependencies, implements any
confirmed gaps, tests them and commits independently. Task 409 requires the
service ports before startup integration. IDs do not imply execution order.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 389 | Close cat V7 compatibility audit | To do | Binary/stdin concatenation, -u, multiple files and read/write failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 390 | Close cp V7 compatibility audit | To do | Overwrite modes, directory destinations, same-inode rejection, permissions and failed copies; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 391 | Close echo V7 compatibility audit | To do | V7 -n prefix handling, literal backslashes, empty arguments and pipeline output; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 392 | Close getty V7 compatibility audit | To do | Speed tables, erase/kill, parity/case, EOF, long names and login handoff on each terminal; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 393 | Close init V7 compatibility audit | To do | rc/single-user transitions, signal handling, reaping, tty reload and getty respawn; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 394 | Close ln V7 compatibility audit | To do | Directory destinations, existing targets, same-inode cases, permissions and hard-link lifetime; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 395 | Close login V7 compatibility audit | To do | Authentication retries, empty/locked passwords, credentials, home/shell environment and failure cleanup; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 396 | Close ls V7 compatibility audit | To do | All reference flags, sorting, directory operands, ownership, time selection and failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 397 | Close mesg V7 compatibility audit | To do | Reference mode/exit status, descriptor selection, non-TTY and ownership failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 398 | Close newgrp V7 compatibility audit | To do | Membership/password rules, credential changes, environment, shell exec and failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 399 | Close passwd V7 compatibility audit | To do | Caller authorization, password checks, database replacement/metadata and failed-update recovery; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 400 | Close pwd V7 compatibility audit | To do | Root/nested paths, directory traversal permissions, renamed directories and errors; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 401 | Close rm V7 compatibility audit | To do | Reference options, recursion, prompting, directories, permissions and open/hard-linked files; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 402 | Close sh V7 compatibility audit | To do | Grammar, expansion, redirection, builtins, pipelines, traps, scripts and login-shell behavior; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 403 | Close stty V7 compatibility audit | To do | Every reference operand/query, atomic settings, raw/cooked transitions and non-TTY/errors; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 404 | Close su V7 compatibility audit | To do | Root/non-root authentication, identity/group changes, environment and shell-exec failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 405 | Close tty V7 compatibility audit | To do | Reference -s, descriptor selection, output/status and non-TTY failures; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 406 | Close wall V7 compatibility audit | To do | User/session selection, message framing, write permissions and unavailable terminals; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 407 | Close who V7 compatibility audit | To do | utmp/wtmp formats, who am i, alternate files, time display and malformed records; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 408 | Close write V7 compatibility audit | To do | Recipient/terminal selection, permissions, message/EOF framing, signals and disconnects; compare supplied source with active port, resolve API/ABI/runtime dependencies, add missing behavior tests and document native adaptations; implement/test/commit independently |
| 409 | Integrate V7 background services into multi-user startup | To do | After 237/238/242–245: integrate update/cron in rc, install a native crontab and service ownership, exercise scheduled atrun and calendar/dmesg consumers, cold boot and single-user/re-entry without duplicate daemons; preserve native configuration during upgrades |
| 410 | Require task ownership for unresolved inventory work | Done | Explicit unresolved_tasks validated against pending component owners; scope_notes separate agreed limits; negative regression tests and make test-inventory; Git history |
| 411 | Reconcile reference review and roadmap documentation | Done | Stale counts/completion claims corrected; audit findings and task mappings published; canonical requirement-table/review-total regression checks and make test-inventory; Git history |

### TCP/IP implementation (0.2.0)

All tasks follow the [networking contract](networking-roadmap.md), including
single-core QEMU/e1000, static IPv4 and blocking sockets. Dependencies below
identify prerequisite capabilities; driver and ABI work can proceed independently
where dependencies permit. DHCP, IPv6, nonblocking sockets and select/poll
remain deferred; name resolution follows in tasks 444–455. Each task requires focused tests, affected regressions and a commit.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 412 | Pin and audit 4.4BSD-Lite2 networking sources | Done | Pinned mirror commit 50587b00e922225c62f1706266587f435898126d; 102 byte-identical reference files, archive/per-file hashes and retained Berkeley licenses; [dependency/LP64 audit](bsd-networking-audit.md); make test-bsd-reference test-inventory, full pinned-archive comparison and rejection fixtures; Git history |
| 413 | Adapt BSD networking types and compatibility primitives | Done | Private namespaced LP64 types, pointer-free wire records, unaligned byte-order codecs, streaming checksum/pseudoheader, modular sequence comparisons and checked size arithmetic; [compatibility contract](networking-compat.md); make test-net-compat (ASan/UBSan, isolated freestanding headers), make check and make test-inventory; Git history |
| 414 | Implement bounded mbuf and socket-buffer pools | Done | Page budgets, fixed-size slabs, 256-byte mbufs, shared 2048-byte clusters, chain operations with rollback, socket buffers with byte/mbuf limits and drop/allocation counters, separate socket-object budget; [pool contract](networking-pools.md); make test-net-pool (ASan/UBSan, injected page failures) and make test-net-pools-guest (exhaustion and leak-free teardown at 16/128 MiB); Git history |
| 415 | Add serialized network worker and protocol timers | Done | One schedulable kernel worker process serializes all network work and timers; bounded coalescing intrusive work queue (64), IRQ-safe non-sleeping submit/wakeup, blocking net_worker_call for socket callers, absolute-deadline one-shot timers replacing BSD fast/slow timeouts; [worker contract](networking-worker.md); make test-net-sched (ASan/UBSan) and make test-net-worker-guest (concurrent sleeping callers, IRQ submit/coalescing, overflow/cancel, timer order/cancel/rearm/periodic expiry, stop and reaping at 16/128 MiB); Git history |
| 416 | Add PCI discovery for QEMU e1000 | Done | Configuration-mechanism-1 scan of all buses/multifunction devices, BAR type/size decoding with decode disabled, legacy INTx pin/IRQ routing, verified command enables (memory, bus master, INTx), uncached supervisor-only NX MMIO window shared by all address spaces; 82540EM discovery and BAR0 mapping; [PCI contract](networking-pci.md); make test-pci (ASan/UBSan simulated bus) and make test-pci-guest (e1000 found and register-read, and no-NIC boots, at 16/128 MiB); Git history |
| 417 | Implement e1000 DMA transmit and receive | Done | Reset/init, EEPROM MAC and link state, 32-entry legacy RX/TX rings with 2 KiB copy buffers, ICR-acknowledging IRQ handler with worker handoff via generic irq_attach, bounded per-pass service, DMA-safe stop/free; [driver contract](networking-e1000.md); make test-e1000 (RX validation, ASan/UBSan) and make test-e1000-guest (ARP and 100 ICMP echoes of 60–1514 bytes through QEMU user networking, ring wrap, full TX/RX rings, malformed TX and synthesized RX descriptors, stop/restart and page accounting at 16/128 MiB, no-NIC boots); Git history |
| 418 | Port Ethernet and loopback interfaces | Done | Bounded interface registry with attach/detach/up/down and BSD-style counters, ethertype protocol demultiplexing, Ethernet header prepend/validate/filter (own address and broadcast), lo0 with a bounded deferred queue (never recursive delivery), em0 bound to the e1000 driver, live net_start/net_stop pool and worker; [interface contract](networking-interfaces.md); make test-net-if (ASan/UBSan) and make test-net-if-guest (lo0 queue bound and single ownership, em0 states, MTU refusal and ARP exchange, restart and page accounting at 16/128 MiB, no-NIC boots); Git history |
| 419 | Port static IPv4 routing and ARP | Done | One validated address/netmask per interface (contiguous masks, unicast only, no network/broadcast host below /31, 127/8 only on loopback, no overlapping subnets), connected routes by longest prefix, one on-link default gateway, own addresses via lo0 and directed broadcast; 16-entry ARP table with 4 held packets per entry and 16 in total, 1 s retries up to 5 then a 20 s negative entry, 20 min expiry, LRU eviction of resolved entries only, in-place replies, flush on down/detach/readdress; [routing and ARP contract](networking-routing.md), `make test-net-route` and guest gateway/unreachable checks (`make test-net-route-guest`); Git history |
| 420 | Port IPv4 input/output and fragment handling | Done | Header version/length/checksum/total-length/source validation with padding trimmed and options passed up, forwarding disabled (own addresses, broadcasts and loopback 127/8 only), up to 4 transport protocols, output with DF/TTL/TOS and all-or-nothing fragmentation at (MTU−20)&~7, reassembly bounded to 8 datagrams, 16 fragments each and 64 in total with 30 s expiry, whole-datagram discard on overlap or inconsistent ends, discard on interface change; [IPv4 contract](networking-ip.md), `make test-net-ip` and guest loopback fragmentation (`make test-net-ip-guest`); Git history |
| 421 | Port ICMP and privileged raw echo sockets | Done | Echo replies (no broadcast echoes, options not reflected), protocol-unreachable and needfrag errors quoting header + 8 bytes with RFC 1122 suppression (non-first fragments, broadcasts, non-unicast sources, ICMP errors), received errors validated and mapped to results for up to 4 transports, raw ICMP sockets limited to privileged callers, 8 sockets with 8192-byte bounded queues, bind/connect filtering and kernel-computed checksums; user-space access through tasks 424–426; [ICMP contract](networking-icmp.md), `make test-net-icmp` and guest loopback/gateway ping and unreachable checks (`make test-net-icmp-guest`); Git history |
| 422 | Port UDP protocol control blocks and datagrams | Done | 32 control blocks; bind once with privileged ports below 1024, BSD wildcard conflict rules and mutual SO_REUSEADDR; round-robin ephemeral ports 49152–65535; connect/disconnect; sends of 0–9216 bytes with pseudoheader checksums; input validation of length, truncation and optional checksums, fewest-wildcard demultiplexing, broadcast copies, ICMP port unreachable for unmatched unicast; ICMP errors reported to connected senders; 41600-byte receive queues with mbuf caps; [UDP contract](networking-udp.md), `make test-net-udp` and guest loopback round trip and refusal checks (`make test-net-udp-guest`); Git history |
| 423 | Port TCP state machine and retransmission | Done | RFC 793 states in 32 control blocks; MSS-only options; RFC 6528 initial sequence numbers; active/passive open with SYN retries, backlog-bounded listen queues and embryonic expiry; 16 KiB stream buffers, Nagle, RFC 6298 RTO with backoff and 12-retry timeout, Reno slow start/avoidance/fast retransmit, zero-window persist probes; window trimming, 16-segment reordering queue, non-shrinking advertised window; RFC 5961 reset/SYN challenges; half-close, CLOSING, LAST_ACK, 60 s TIME_WAIT and FIN_WAIT_2 timeout, reset on close with unread data; ICMP needfrag MSS reduction, refused connects and soft errors; [TCP contract](networking-tcp.md), `make test-net-tcp` (deterministic packets plus lossy/reordered loopback transfers) and guest loopback echo (`make test-net-tcp-guest`); Git history |
| 424 | Define native socket ABI and libc interfaces | Done | Syscalls 77–88 and errno 36–57 appended (4.4BSD order and messages, EWOULDBLOCK=EAGAIN); fixed-width 16-byte sockaddr/sockaddr_in, 128-byte storage and 16-byte timeval in `<nexix/socket.h>`; `<sys/socket.h>`, `<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<sys/time.h>` installed with 14 wrappers, byte-order helpers and numeric IPv4 conversions; V7 mpx connect moved to mpx_connect behind a `<sys/mx.h>` macro; kernel boundary validates families, types, protocols, raw privilege, lengths, flags, options and pointers before descriptors; [socket ABI](sockets.md), `make test-header-abi`, `make test-inet-unit` and ring-3 checks in `make test-libc`; Git history |
| 425 | Integrate sockets with native file descriptors | Done | Socket objects from the 16-page budget behind ordinary open files (S_IFSOCK fstat, ESPIPE, ENOTTY, kinfo kind 10); the first socket starts the stack detached with lo0 at 127.0.0.1/8; bind/listen/accept/connect/shutdown/getsockname/getpeername and stream read/write with checked user copies, interruptible lost-wakeup-free waits, partial transfers, EOF, ECONNRESET then EPIPE with SIGPIPE; fork/dup sharing, close-on-exec and final-close/exit release; EMFILE/ENFILE/ENOBUFS; datagram/raw I/O, send/recv/sendto/recvfrom and options left to 426; [socket descriptors](sockets.md#descriptors-task-425) and `make test-socket-api` at 16/128 MiB; Git history |
| 426 | Complete blocking socket operations and options | Done | send/recv/sendto/recvfrom over TCP, UDP (whole datagrams, truncation, EDESTADDRREQ/EISCONN/EMSGSIZE, ICMP errors) and raw ICMP (IP header included); MSG_PEEK/WAITALL/DONTWAIT/NOSIGNAL; O_NONBLOCK on sockets (EAGAIN, EINPROGRESS, EALREADY); SO_REUSEADDR, SO_SNDBUF/RCVBUF (512–262144), SO_SNDTIMEO/RCVTIMEO (tick-rounded, EAGAIN or partial counts), SO_ERROR, SO_TYPE, TCP_NODELAY; unsupported families/options/flags rejected; [operations and options](sockets.md#operations-and-options-task-426) and `make test-socket-api` (EINTR, refusal, reset, timeouts) at 16/128 MiB; Git history |
| 427 | Add privileged interface and route configuration tools | Done | NX_SIOCGIFINFO/SIFCONFIG/GROUTE/SROUTE/LOOKUP socket ioctls with fixed records, root-only changes (EPERM) and open queries; whole-or-nothing interface changes with address rollback when up fails; installed /etc/ifconfig (list/show, address with /bits, netmask or classful default, up/down/delete) and /etc/route (connected and default table, add/delete default, get); malformed text and kernel refusals reported with exit 2/1; [configuration contract](netconfig.md), `make test-net-config` and `make test-netconfig` (16/128 MiB with e1000, no NIC, non-root denial); Git history |
| 428 | Add numeric-address ping and TCP/UDP nc | Done | Setuid-root /bin/ping (drops root after opening its raw socket; -c/-i/-W/-s, late and duplicate replies, unreachable/time-exceeded reports, SIGINT statistics, exit 0/1/2) and /bin/nc (TCP/UDP client and one-shot listener, half-close at end of input, -w connect/accept/idle timeouts, signals stop both processes); [command contract](nettools.md) and `make test-nettools` at 16/128 MiB (loopback and gateway echo, absent host, payloads both ways with a binary round trip, UDP replies, refusal, timeouts, setuid use, SIGINT/SIGTERM, usage errors); Git history |
| 429 | Integrate optional network startup and QEMU profile | Done | init runs an optional /etc/netstart with sh after /etc/rc (works on upgraded disks that keep their rc); without it only lo0 is configured; installed /etc/netstart.qemu sample; make NIC=user or none and HOSTFWD="proto:host:guest ..." on every run target with validation; [startup and profile](netstart.md), `make test-qemu-profile`, upgrade preservation in `make test-upgrade-unit` and `make test-netstart` (cold boots with and without netstart, no NIC, host forwarding both ways at 16/128 MiB); Git history |
| 430 | Complete networking host robustness tests | Done | Whole-stack ASan/UBSan test (ARP, IPv4 fragments, ICMP, UDP, TCP, raw) with 40000 deterministic mutations of valid frames per seed and checksums often repaired, an intact TCP transfer through a pool far too small, fragment floods within reassembly bounds, timer-only return to baseline after three hours of expiry, and teardown with traffic in flight; 32 seeds per run (2000 swept clean); `make test-net-host` runs every network sanitizer test; [robustness contract](networking-robustness.md) and `make test-net-robust`; Git history |
| 431 | Complete TCP/IP guest acceptance and documentation | Done | `make test-tcpip-acceptance`: disposable 16/128 MiB guests configured by /etc/netstart cover loopback, static IPv4, ARP (gateway and absent neighbour), ping, UDP and outbound TCP (with a binary round trip) against host echo services on 127.0.0.1 only, refusal and inbound TCP through a host forward both ways; `make test-tcpip` adds every network suite (sanitizer, socket descriptors with signals/fork/dup/cleanup, configuration, tools, startup, no-NIC boots) and the libc/syscall/process/file/signal/console/inventory regressions; [networking guide](networking.md) with setup, ABI, limits, installed files and evidence; Git history |

### System directory layout (0.2.0)

V7 kept system binaries in `/etc`; 4.4BSD moved them to `/sbin`, `/usr/sbin`
and `/usr/libexec`, leaving `/etc` for configuration and rc scripts. Tasks
436–443 close the remaining differences recorded in the
[hierarchy guide](hier.md#differences-from-44bsd). Nexix
adopts that rule, including BSD's `/usr/bin` for `wall`. No compatibility
links stay in `/etc`.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 432 | Relocate system binaries to BSD directories | Done | init, dmesg, mknod, ifconfig and route in /sbin; accton and update in /usr/sbin; getty in /usr/libexec; wall in a new /usr/bin; nothing executable left in /etc; the kernel starts /sbin/init and, only for disks not yet upgraded, /etc/init with a ROOT OLD LAYOUT warning; init starts /usr/libexec/getty and its recovery shell, like root's profile, uses PATH=/sbin:/bin:/usr/sbin:/usr/bin (users /bin:/usr/bin); bootstrap image, netstart sample, inventory install fields, tools, tests and docs updated; the seven V7 manuals naming these binaries now give the new paths, checked to differ from the originals by exactly those paths (`make test-man-pages`); getty, wall, accton, mknod, update, dmesg, multi-user, terminal, console, syscall, libc, networking and boot suites at 16/128 MiB; Git history |
| 433 | Retire relocated paths during disk upgrades | Done | The installation manifest's validated `retired` list names the nine old /etc binaries with their replacements; upgrades remove each that is still a regular file (other hard links keep the old bytes), keep anything else there under `retired_kept`, and warn about kept /etc files and profiles that still name a retired path and about a root PATH without /sbin; [preservation policy](disk-upgrades.md#preservation-policy), `make test-upgrade-unit` (removal, kept directory, hard link, warnings, validation) and `make test-layout-upgrade`: a V7-layout disk with the pre-432 init boots with the ROOT OLD LAYOUT warning, upgrades with exactly those removals and warnings, then boots from /sbin/init with no binaries left in /etc at 16/128 MiB; Git history |
| 434 | Verify and document the BSD directory layout | Done | `make test-etc-layout`: both installation manifests and the built distribution image keep only non-executable configuration in /etc (10 files), every ELF program in /bin, /sbin, /usr/bin, /usr/sbin, /usr/libexec, /usr/lib or /lib, and no retired path; the layout upgrade test scans the upgraded image the same way; [file system hierarchy](hier.md) with the directory roles, PATHs, the V7-to-BSD move table and the remaining V7 differences (user commands in /bin, compiler passes in /lib); `make test-tcpip` (now with the layout checks), the moved commands' suites, manuals, upgrades and the unit suite pass; Git history |
| 435 | Move ping and nc to BSD directories | Done | /sbin/ping (still setuid root) and /usr/bin/nc, with /bin/ping and /bin/nc in the manifest's retired list so upgrades remove them; every profile and the recovery shell use PATH=/sbin:/bin:/usr/sbin:/usr/bin, as in FreeBSD; inventory, tools, tests and docs updated; `make test-netstart` now has an ordinary user run ping and nc by name; ping/nc, TCP/IP acceptance, netstart, layout upgrade, upgrade, terminal, getty, wall, layout and unit suites at 16/128 MiB; Git history |
| 436 | Remove the duplicate /bin/dmesg link | To do | After 435: /sbin/dmesg only, with /bin/dmesg retired on upgrade; dmesg, layout and upgrade suites |
| 437 | Introduce /usr/share for architecture-independent data | To do | After 435, checked against the retained 4.4BSD-Lite2 hier(7) and mtree specs: /usr/man to /usr/share/man (source manN pages, as later BSDs; 4.4BSD shipped only formatted catN), /usr/pub/ascii and eqnchar to /usr/share/misc (greek too, without a 4.4BSD counterpart), /usr/doc to /usr/share/doc, /usr/lib/tmac to /usr/share/tmac, the V7 troff font and nroff terminal tables to /usr/libdata/troff_font and /usr/libdata/term, yaccpar and lex forms to /usr/share/misc and the lint libraries to /usr/libdata/lint (neither has a 4.4BSD counterpart); the termcap database to /usr/share/misc/termcap, deciding whether /etc/termcap and /usr/lib/tmac remain links as in the 4.4BSD skeleton; update man, nroff/troff, eqn, yacc, lex, lint and termcap readers, the manual fidelity test and the upgrade retirement list; manual, formatter, compiler-tool, layout and upgrade suites at 16/128 MiB |
| 438 | Move variable data to /var | To do | After 435: add /var/log, /var/run, /var/account, /var/mail, /var/spool and /var/db; move wtmp to /var/log/wtmp, utmp to /var/run/utmp and the accounting file convention to /var/account/acct; update login, init, who, write, wall, ac/sa and accton, retire the old paths on upgrade while keeping their records; session, accounting, layout and upgrade suites at 16/128 MiB |
| 439 | Retire /lib for /usr/lib and /usr/libexec | To do | After 435: compiler passes (cpp, c0, c1, c2, fc1, as2, ccom, lint1, lint2) and the lorder helper to /usr/libexec (4.4BSD used /usr/libexec/gcc2, with a /usr/bin/cpp wrapper script, which Nexix may add); libc.a, libm.a, libln.a and the startup objects to /usr/lib; update the cc, pcc, as, ld and lint drivers, the development kit and toolchain ABI tests, and the upgrade retirement list; compiler, toolchain, development, layout and upgrade suites at 16/128 MiB |
| 440 | Split user commands between /bin and /usr/bin | To do | After 435: keep in /bin what 4.4BSD's bin/Makefile installs there (Nexix has cat, chmod, cp, date, df, echo, ed, expr, hostname, kill, ln, ls, mkdir, mv, ps, pwd, rm, rmdir, sh, sleep, stty, sync, test and [) and move the rest to /usr/bin, with chown to /usr/sbin and chgrp a /usr/bin link to it as in usr.sbin/chown/Makefile; retire the old paths on upgrade; check scripts, profiles and tools that name /bin paths; every command, shell, development and upgrade suite at 16/128 MiB |
| 441 | Add /mnt and /usr/local | To do | After 435: create /mnt (temporary mounts) and /usr/local with bin, sbin, lib, libexec, libdata, include and man as in the 4.4BSD usr spec and hier(7), with /usr/local/bin after the system directories in PATH; layout and upgrade suites |
| 442 | Split password hashes into /etc/master.passwd | To do | After 435: a root-only master.passwd holding hashes and a public passwd without them, as in 4.4BSD (with or without pwd.db/spwd.db databases); update login, su, passwd, newgrp and the account readers, and migrate existing accounts on upgrade without changing IDs or hashes; account, login, upgrade and security suites |
| 443 | Adopt BSD device names | To do | After 435: decide and implement 4.4BSD i386 names from etc.i386/MAKEDEV (wd*/rwd* disks, fd* floppies, com* serial ports, wt*/rwt* tape) for the Nexix devices, alongside or instead of the V7 names, with mount, tape, getty/ttys and storage tools updated; device, storage, terminal and upgrade suites |

### Name resolution (0.2.0)

4.4BSD-Lite2 resolves names in libc: `gethostnamadr.c` asks the name servers
in `/etc/resolv.conf` through the BIND 4.9 stub resolver (`res_init.c`,
`res_query.c`, `res_send.c`, `res_mkquery.c`, `res_comp.c`) and reads
`/etc/hosts` only when no name server answers at all (`ECONNREFUSED`);
`/etc/services`, `/etc/protocols` and `/etc/networks` are read by their own
`get*ent` routines. Tasks 444–455 port that client side to Nexix's blocking
IPv4 sockets. Without `select`, the resolver waits with `SO_RCVTIMEO`; with no
`nameserver` line it asks 127.0.0.1 (Lite2 sent to `INADDR_ANY`, which Nexix
does not route), so a guest without a local server still falls back to
`/etc/hosts` at once through the ICMP port-unreachable error. The `named`
server, `named-xfer`, `dig`, `host`, `rcmd`/`hosts.equiv`, IPv6 records,
EDNS0 and DNSSEC stay out of scope; tests never depend on the public
Internet.

| Task | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 444 | Pin and audit the 4.4BSD-Lite2 resolver sources | Done | [Source audit](resolver-audit.md); 167 hash-pinned files, additive provenance migration; `make test-bsd-reference test-inventory`; Git history. Scope: Add to the pinned reference set: `lib/libc/net` (gethostnamadr, sethostent, herror, res_comp, res_debug, res_init, res_mkquery, res_query, res_send, get{serv,proto,net}* and inet_{network,makeaddr,lnaof,netof} with their section 3 manuals), `include/{netdb,resolv}.h`, `include/arpa/nameser.h`, `etc/{hosts,services,protocols,networks}`, `share/man/man7/hostname.7` and BIND 4.9.2's `resolver.5` and `tools/nslookup`; record the LP64 and safety findings (`(u_int32_t)bp` alignment cast, HEADER bit-field order, unchecked `cp + n` against the end of message in getanswer, sequential `++_res.id` query IDs, replies accepted from any source, `gethostent` declared but never defined, `select`/`writev` dependencies, static result buffers); amend the [networking contract](networking-roadmap.md) to move the stub resolver from deferred into 0.2.0 with the limits above; `make test-bsd-reference test-inventory` |
| 445 | Add `<netdb.h>` and the services, protocols and networks databases | Done | [Native contract](netdb.md); `make test-netdb-unit test-header-abi test-libc test-libc-interfaces test-inventory`; Git history. Scope: After 444: `<netdb.h>` (hostent, netent, servent, protoent, `h_errno`, HOST_NOT_FOUND/TRY_AGAIN/NO_RECOVERY/NO_DATA, `_PATH_*`), `herror`/`hstrerror`, `get{serv,proto,net}ent`, `set*ent`/`end*ent`, `getservbyname`/`getservbyport`, `getprotobyname`/`getprotobynumber`, `getnetbyname`/`getnetbyaddr` and `inet_network`/`inet_makeaddr`/`inet_lnaof`/`inet_netof`; install `/etc/services`, `/etc/protocols` and `/etc/networks` from the Lite2 files; comments, alias limits, long and malformed lines, port byte order and stayopen rewind; manuals; host unit tests (ASan/UBSan), ring-3 `make test-libc` checks and `make test-header-abi` |
| 446 | Read `/etc/hosts` | Done | [Hosts contract](netdb.md); `make test-hosts-unit test-netdb-guest test-libc test-libc-interfaces test-inventory`; Git history. Scope: After 445: Lite2's `_sethtent`/`_gethtent`/`_gethtbyname`/`_gethtbyaddr` with case-insensitive name and alias matches, plus the documented but missing public `gethostent`; reject lines whose address fails `inet_aton` instead of storing `INADDR_NONE`; 35 aliases, overlong lines and comments; install a default `/etc/hosts` with `127.0.0.1 localhost`; `hosts(5)`; host unit tests and ring-3 libc checks |
| 447 | Port the DNS message codec | Done | [Codec contract](resolver.md); `make test-dns-codec test-header-abi test-libc-interfaces test-libc test-inventory`; 40000 ASan/UBSan mutations; Git history. Scope: After 444: `<arpa/nameser.h>` with x86_64 bit-field order and the RFC 1035 constants, `dn_comp`, `dn_expand`, `dn_skipname`, `_getshort`/`_getlong`/`putshort`/`putlong`, `res_mkquery` and the `res_debug` printers (`p_type`, `p_class`, `fp_query`) used by `RES_DEBUG` and nslookup; every read bounded by the message end, compression-pointer loops and forward pointers, labels over 63 and names over 255 bytes rejected; host unit tests with RFC 1035 vectors and 40000 seeded mutations per run under ASan/UBSan, as in task 430 |
| 448 | Provide unpredictable resolver query IDs | Done | [Entropy contract](syscall-abi.md); CPUID-gated RDRAND, syscall 89, fail closed without entropy; `make test-entropy-unit test-entropy-guest test-dns-codec test-header-abi test-libc test-syscall-policy-unit test-inventory`; Git history. Scope: After 444: replace Lite2's sequential `++_res.id` with IDs user space cannot predict; add and document a native entropy interface (for example a `getentropy`-style system call keyed like the TCP initial sequence numbers of task 423) or justify an alternative; ABI records, `make test-header-abi`, host and ring-3 tests for distribution and repeated calls across fork |
| 449 | Read `/etc/resolv.conf` | Done | [Configuration contract](resolver.md); `make test-res-init test-res-init-guest test-libc-interfaces test-inventory`; Git history. Scope: After 445: `<resolv.h>`, `_res` and `res_init` with `domain`, `search` (6 domains) and up to 3 `nameserver` lines, `LOCALDOMAIN`, the default domain taken from the full name in [/etc/hostname](hostname.md), the derived search list (`LOCALDOMAINPARTS`), Lite2's 5 s timeout and 4 attempts, and 127.0.0.1 as the default server; comments, invalid addresses, overlong lines and missing files; `resolver(5)`; host unit tests with a path override and ring-3 checks |
| 450 | Port the resolver query transport | To do | After 447–449: `res_send`, `res_query`, `res_search`, `res_querydomain` and `HOSTALIASES`; UDP waits through `SO_RCVTIMEO` with Lite2's back-off (`retrans << try`, divided by the server count), a connected socket for one server (ECONNREFUSED means no server) and `recvfrom` for several; accept only replies from the queried server's address and port whose ID and question match; SERVFAIL/NOTIMP/REFUSED skip that server; a truncated reply retries over TCP with the length prefix and message in one write (no `writev`), flushing an oversized answer and requerying once after ECONNRESET; `RES_STAYOPEN`/`RES_USEVC` and `_res_close`; `h_errno` mapping and EINTR behaviour documented; `resolver(3)`; host tests against a scripted transport and `make test-resolver-guest` against a server bound to lo0 at 16/128 MiB |
| 451 | Port `gethostbyname` and `gethostbyaddr` | To do | After 446 and 450: Lite2 `getanswer` with every length checked against the message end, LP64-correct alignment of copied addresses, CNAME aliases, up to 35 addresses and aliases, PTR answers for in-addr.arpa; all-numeric names answered without a query unless they end in a dot; Lite2 order (DNS, then `/etc/hosts` only on ECONNREFUSED) and `sethostent`/`endhostent` keeping a TCP connection; `gethostbyname(3)`; host tests and the guest server checks of task 450 |
| 452 | Accept host and service names in network commands | To do | After 451: `ping`, `nc`, `ifconfig` and `route` accept names as well as numeric addresses (4.4BSD's netstart passed `$hostname` to ifconfig), `nc` accepts service names through `getservbyname`, and `ping -n` selects numeric output while replies are otherwise shown with names as in 4.4BSD; unknown names exit 2 with an `herror` message; [command contract](nettools.md), [configuration contract](netconfig.md) and manuals; `make test-nettools test-netconfig` at 16/128 MiB |
| 453 | Port nslookup | To do | After 451: BIND 4.9.2 `tools/nslookup` as `/usr/sbin/nslookup` with its help file; `commands.l` built with the native lex; `send.c`'s `select` replaced by `SO_RCVTIMEO`; interactive and command-line modes, `server`/`lserver`, `set` options (type, class, domain, search, retry, timeout, vc, debug), SIGINT returning to the prompt, and `ls` zone listings over TCP; `nslookup(8)`; guest tests against the task 450 server at 16/128 MiB |
| 454 | Install resolver configuration in startup, QEMU profile and upgrades | To do | After 449: `/etc/resolv.conf.qemu` (`nameserver 10.0.2.3`, QEMU's DNS) installed like `/etc/netstart.qemu`, with `/etc/resolv.conf` optional; installation manifest entries and upgrades that keep existing `/etc/hosts`, `/etc/resolv.conf`, `/etc/services`, `/etc/protocols` and `/etc/networks`; [startup and profile](netstart.md) and [hierarchy](hier.md) updated; `make test-qemu-profile test-upgrade-unit test-netstart test-etc-layout` |
| 455 | Complete name resolution acceptance and documentation | To do | After 451–454: `make test-resolver-acceptance` with disposable 16/128 MiB guests querying a host DNS stub on 127.0.0.1 (reached as 10.0.2.2, never the public Internet): A records with several addresses, CNAME chains, PTR, NXDOMAIN, NODATA, SERVFAIL, REFUSED, truncation to TCP, replies with a wrong ID or source ignored, a silent server timing out with the documented retries, failover to a second server, the search list, and no server falling back to `/etc/hosts`; whole-resolver host robustness test (ASan/UBSan, seeded mutated answers); `make test-tcpip` includes the resolver suites; [name resolution guide](resolver.md) and the [networking guide](networking.md) updated |

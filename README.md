# Nexix

Current development version: **0.1.0**.

Nexix ports UNIX Seventh Edition (V7) to x86_64 and adds features including TCP/IP.
It preserves V7 source and behavior where practical, with native ELF64 programs
rebuilt from source rather than PDP-11 binary compatibility.

Nexix now boots into the rebuilt V7 Bourne shell on COM1. The kernel mounts a
persistent IDE-backed V7 filesystem and launches `/etc/init` as
PID 1. Init starts the single-user shell; native ELF64 programs execute from
filesystem paths. See [booting Nexix](docs/booting.md).

The kernel provides protected x86_64 address spaces, guarded kernel stacks,
physical-page allocation, scheduling, timer preemption, eager fork, exit/wait
and orphan reparenting. Its native syscall ABI validates user-memory access.
File descriptors support regular files, directories, links, pipes, permissions
and descriptor ownership across fork/exec/exit. COM1 provides cooked editing,
raw/cbreak modes, echo, flow control and controlling-terminal signals.

Native libc supports the retained V7 init, shell and nine utilities. Signals,
alarms, brk/sbrk, credentials, process groups and time calls supply their basic
services. The original V7 reference sources and historical disk encodings are
preserved. `make run` keeps root changes in `build/nexix.img` across boots.
See [persistent storage](docs/storage.md). TCP/IP is planned for version 0.2.0.

## Build and boot

Dependencies: GNU Make, Python 3, an `x86_64-elf` GCC/binutils cross-toolchain,
GRUB's BIOS modules and `grub-mkrescue`, xorriso, and `qemu-system-x86_64`.
On Debian/Ubuntu the runtime tools are provided by `grub-common`, `grub-pc-bin`,
`xorriso`, and `qemu-system-x86`.

```sh
make check
make iso
make run
make test-boot
make test-shell
make test-storage
make smoke
make test
make test-process
make test-syscall
make userland
make test-libc
make test-fs
make test-files
make test-tty
make test-signals
make run-tty
```

For an initial build on an x86_64 Linux host with GCC/binutils, use
`make TOOLCHAIN_PREFIX= check` (and the same override for other targets).
This is a bootstrap convenience; a pinned cross-toolchain remains planned.
The kernel does not link host libc. Build artifacts go into `build/`.

`make clean` deletes the entire `build/` directory, including the persistent
`build/nexix.img` disk and all files saved inside Nexix. Stop QEMU before cleaning.
`make run` displays the shell prompt. Ctrl-D closes the shell and init starts
a replacement; Ctrl-A, release, then X exits QEMU.
`make smoke` boots the `test=selftest` regression ISO with one CPU with 128 MiB RAM under QEMU TCG, requires the serial
boot and timer markers within 15 seconds, and saves `build/boot.log`.
`make test-process` runs process scheduling and lifecycle checks with 16 MiB
RAM; normal `make smoke` also runs them with 128 MiB. See
[processes](docs/processes.md) for implemented behavior and limitations.
`make test-unit` runs host tests for boot-data parsing, the allocator, and V7
native types, disk encodings, safe user-memory copies, and filesystem/device
validation.
`make test` additionally boots separate images with `test=ud` and `test=pf`
command lines. These deliberately trigger invalid-opcode and unmapped-memory
exceptions, verifying the vector, error code, and page-fault address in serial
panic diagnostics. Logs are `build/ud.log` and `build/pf.log`. An unexpected
QEMU exit fails the checks, including a triple fault after a success marker.
It also tests page exhaustion/recovery with 16 MiB and 128 MiB RAM, writes every
allocated page, and verifies that boot information and an 8,193-byte module
remain intact. Logs are `build/memory-16M.log` and `build/memory-128M.log`.
The bootstrap allocator currently manages only RAM below the 1 GiB mapping limit.
`make test-paging` tests read-only writes, NX execution, unmapped and null-page
access, and page-table allocation failure/rollback. These are also included in
`make test`. These paging cases run in kernel mode.
`make test-user` exercises actual user-mode memory protection, privileged
instructions, port I/O, and interrupt-gate permissions. It also verifies kernel
entry with an unmapped user stack, and double-fault diagnostics using an IST
stack when the ordinary entry stack is unusable. These tests are included in
`make test`; `make -j3 TOOLCHAIN_PREFIX= test` runs independent groups concurrently.
Logs are `build/user-*.log`. Deliberate fatal-fault cases print `PANIC`; the
harness requires the expected vector, error, originating privilege, and address.
`make test-syscall` repeats the regression boot tests with 16 MiB RAM and saves
`build/syscall-16M.log`. Every successful regression boot must also print `NEXIX_SYSCALL_OK`
and the supporting ABI, user-copy, error, and fork markers. See the
[native syscall ABI](docs/syscall-abi.md) for register conventions, types, errors,
and supported calls. ELF64 loading and atomic exec replacement are implemented;
see [execution ABI](docs/exec.md). `make TOOLCHAIN_PREFIX= test-exec` runs the
ELF and fork/exec/wait tests with 16 MiB RAM (`build/exec-16M.log`). The current
program catalog includes native assembly and C fixtures. Libc startup and
wrappers and the libc support needed by the rebuilt V7 programs are implemented. `make TOOLCHAIN_PREFIX= test-libc` runs the C userland
tests with 16 MiB RAM (`build/libc-16M.log`).
`make TOOLCHAIN_PREFIX= test-files` exercises file operations, descriptor
ownership, permissions and blocking pipes with 16 MiB RAM (`build/files-16M.log`).
`make TOOLCHAIN_PREFIX= test-tty` exchanges real COM1 input with 16 MiB and
128 MiB RAM and tests an interactive demo. `make TOOLCHAIN_PREFIX= run-tty`
starts that demo: type a line and Enter, use Backspace/Ctrl-U to edit, or type
`quit` to finish. Ctrl-A, release, then X exits QEMU. Normal `make TOOLCHAIN_PREFIX= run` starts the V7 shell.

## Roadmap

Version **0.1.0** ports the complete V7 source distribution to native x86_64:
the kernel's applicable behavior, userland, libraries, development tools,
scripts, manuals, supporting data and standalone facilities. It includes
multi-user startup, RTC-derived time and correct persistent V7 timestamps.

### Current implementation and scope

The reviewed baseline is commit `63e8d0d`. Nexix boots into a working
single-user V7 shell with persistent IDE storage. Eleven native programs are
installed: **init, sh, echo, cat, ls, pwd, cp, ln, rm, date and touch**.
The inventory accounts for **217 components, 2,144 distribution files and
35 supporting/platform requirements**. Its component statuses are 11 implemented,
2 partial, 1 prototype and 203 not ported; importing reference inputs does not
complete their native ports.

The expanded roadmap below has **364 tracking steps: 40 done, 2 in progress
and 322 to do** after step 40. These counts are a work breakdown, not an
estimate of the percentage of porting effort completed. Steps 1–24 retain
their previous scope. Completed portions of the old step 25 are separated
into steps 25–36; its remaining work is listed individually.

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

Known gaps include set-ID execution, core dumps, multiplexed channels, complete
accounting/tracing/profiling, raw devices and mount administration. Packaging
currently has a 64-inode image builder and a fixed 512 KiB embedded RAM root;
steps 83–86 address these limits before full-distribution installation.

See the [porting inventory](docs/porting-inventory.md) and its
[component manifest](docs/v7-userland-manifest.json) for original inputs,
outputs, dependencies, acceptance checks and missing sources. The
[roadmap coverage map](docs/roadmap-coverage.json) and
[coverage contract](docs/roadmap-coverage.md) connect every component and
requirement to these step IDs; test-inventory validates the relationships. Older step
numbers in detailed documents refer to the previous roadmap and are mapped below.

### Implementation, testing and commit rules

Each future step follows one implementation, test and commit cycle:

1. Implement the stated capability.
2. Run focused acceptance tests and affected regressions.
3. Update the inventory, documentation and step status.
4. Commit the step and record its hash and test evidence.

**Done** means implemented, tested and committed. **In progress** means relevant
implementation exists but complete stated coverage is unfinished. **To do**
includes blocked work, with the blocker recorded in notes.

Historical commits sometimes contain several features. The tables record their
actual shared commits; the expanded numbering does not rewrite Git history.
Numbers are stable sequential tracking IDs. Execute dependency-ready steps;
source recovery and independent ports may proceed before lower-numbered
dependencies finish.

Each component must reference its owning steps, prerequisites, native outputs,
acceptance checks and commits. Every source, header, assembly file and build
input must map to an adaptation, supporting asset or documented native equivalent.
Compilation alone, preserved binaries or an ENOSYS stub do not complete a port.

Unless a row states otherwise, a program-port step includes native build
integration, installation, source-defined behavior, exit/error cases and guest
acceptance fixtures. Cover all applicable outputs, including scripts, private
helpers, libraries, generated data and optional targets. Large components are
split into smaller steps below. Most steps restore V7 functionality; native
ELF/x86_64 ABI and device equivalents are platform adaptations.

### Previous roadmap cross-reference

| Previous roadmap | Expanded steps |
|---|---|
| 1–24: implemented foundation | 1–24 |
| 25: kernel/libc prerequisites and floating point | 25–82 |
| 26: independent terminals | 89–93 |
| 27: multi-user startup | 94–108 |
| 28: full userland/toolchain port | Component steps in 87–88 and 109–355 |
| 29: packaging and release | 83–86 and 356–361 |
| Roadmap and coverage maintenance | 37 |

### Implemented foundation

| Step | Description | Status | Evidence / notes / acceptance |
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

### Completed portions of old step 25

These steps are complete within their documented native contracts; full libc/libm
coverage and installation remain subject to later acceptance steps.

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 25 | Initialize and isolate x87/MMX/SSE/SSE2 state | Done | `8119d4f`; defined native contracts, not full step 25 |
| 26 | FP state across scheduling, fork and exec | Done | `8119d4f`; defined native contracts, not full step 25 |
| 27 | FP signal contexts and setjmp/longjmp controls | Done | `8119d4f`; defined native contracts, not full step 25 |
| 28 | Binary64 helpers and decimal conversion/formatting | Done | `8119d4f`; defined native contracts, not full step 25 |
| 29 | Adapt all 16 supplied V7 libm C sources | Done | `8119d4f`; defined native contracts, not full step 25 |
| 30 | mknod and special-inode lifecycle | Done | `8119d4f`; defined native contracts, not full step 25 |
| 31 | Privileged directory link/unlink semantics | Done | `8119d4f`; defined native contracts, not full step 25 |
| 32 | Per-process chroot | Done | `8119d4f`; defined native contracts, not full step 25 |
| 33 | Native scanf family | Done | `5098483`; implemented subset |
| 34 | Stream helpers and pushback | Done | `5098483`; implemented subset |
| 35 | Character/string/integer helpers and formatting extensions | Done | `5098483`; implemented subset |
| 36 | Buffered stdio, setbuf and exit/close flushing | Done | `63e8d0d`; old 25.8 |

### Remaining kernel and libc prerequisites

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 37 | Publish renumbered roadmap and enforce component-to-step coverage | Done | test-inventory validates 217 components, 35 requirements and 361 steps; commit recorded in Git history |
| 38 | Complete kernel source adaptation coverage | Done | [Source and syscall audit](docs/kernel-coverage.md); test-kernel-coverage; Git history; kernel port remains incomplete |
| 39 | Complete exported native V7 header/ABI coverage | Done | [Header contracts and deferred owners](docs/header-coverage.md); test-header-abi; Git history; feature implementations remain with their steps |
| 40 | Finish libc scalar/byte helpers and conversion contracts | Done | [V7 helper contracts](docs/libc-helpers.md); test-libc-helpers and test-libc-interfaces; Git history; libc remains partial |
| 41 | Adapt internal FILE callers and historical remote printf convention | To do | Keep native va_list and source adapters; no PDP-11 stack interpretation |
| 42 | Temporary-file routines | To do | V7 mktemp and required consumers |
| 43 | Complete error messages and perror interfaces | To do | V7 error numbering and exported tables |
| 44 | Password database interfaces | To do | Iteration, getpw/getpwnam/getpwuid and malformed records |
| 45 | Group database interfaces | To do | Iteration and group membership |
| 46 | V7 crypt/password primitives | To do | Known-answer historical DES fixtures |
| 47 | getpass and tty/account lookup | To do | Echo restoration, ttyname/ttyslot/getlogin |
| 48 | system | To do | V7 shell execution, signals and child status |
| 49 | popen/pclose | To do | Pipe directions, descriptor ownership and exact child collection |
| 50 | Set-UID/set-GID executable semantics | To do | Currently explicitly rejected; atomic credentials and failure rollback |
| 51 | Executable text ownership and ETXTBSY behavior | To do | Native resident text equivalent; open/write/exec/fork/exit lifetime |
| 52 | Native core dumps | To do | ELF register/memory image; V7 signal status and set-ID restrictions |
| 53 | Raw block/character storage devices | To do | Major/minor routing, bounded raw I/O and device errors |
| 54 | Multi-volume namespace and mount | To do | Mount crossings, dotdot, chroot and inode identity |
| 55 | Unmount and busy-volume handling | To do | Open descriptors, cwd/text references and sync/error behavior |
| 56 | Kernel process accounting | To do | acct, exit records, privilege and storage failure |
| 57 | ptrace stop/continue and process access | To do | Permissions, wait integration and native register access |
| 58 | ptrace single-step and debugger memory changes | To do | Instruction stepping and executable-page changes under tracing |
| 59 | profil sampling | To do | Native PC-to-user-buffer sampling and bounds |
| 60 | mcount, monitor and profiling startup objects | To do | mcrt0/fmcrt0 and native profiling records |
| 61 | ELF nlist and executable symbol interfaces | To do | Shared native symbol representation for diagnostics |
| 62 | Kernel-information snapshots | To do | Versioned process/file/inode/mount/memory/I/O data; avoid historical offsets |
| 63 | Resident-memory lock interface | To do | No disk swap; V7 permissions and resident-page behavior |
| 64 | Privileged phys interface | To do | Native mapping equivalent; range/ownership and rollback tests |
| 65 | Multiplexed group/channel creation and ownership | To do | mx1/mx2 identifiers, descriptors and lifecycle |
| 66 | Multiplexed data/control events and flow control | To do | Read/write formats, bounded queues and wakeups |
| 67 | Multiplexed TTY bridging and libc mpx wrappers | To do | Original consumers; no ENOSYS substitute |
| 68 | V7 serial packet line discipline | To do | pk0–pk3 and libc pkon/pkoff; serial, not TCP/IP |
| 69 | Native memory/special-device interfaces | To do | mem/sys equivalents with explicit native access rules |
| 70 | Historical disk-controller equivalents | To do | hp/rf/rk/rl/rp mapped to native block media |
| 71 | Sequential tape-device equivalent | To do | ht/tm control operations and file-backed tape marks |
| 72 | DECtape-device equivalent | To do | tc random-block tape behavior and format fixtures |
| 73 | Disk ordering/interleave helpers | To do | dsort/dkleave algorithms with native device queues |
| 74 | Historical terminal-controller equivalents | To do | dc/dh/dz/kl and parity mapped to native TTYs |
| 75 | Modem and dial-control equivalents | To do | dn/dhdm/dhfdm/du controls on testable serial endpoints |
| 76 | Typesetter device equivalent | To do | cat driver output protocol captured on native endpoint |
| 77 | Printer/plotter device equivalents | To do | vp/vs output/control with captured protocol fixtures |
| 78 | Native kernel configuration/build tooling | To do | mkconf and kernel recipes; existing bootstrap remains |
| 79 | Resolve V6 wrappers and PDP-11 runtime assembly replacements | To do | Explicit native mappings, including compiler arithmetic/prologues |
| 80 | Complete native libm coverage and source-gap resolution | In progress | libm built; header-only gamma and missing inputs cannot be silently claimed |
| 81 | Document/test native replacement for libfpsim/fakfp | To do | Hardware FP equivalent, not PDP-11 binary emulation |
| 82 | Final libc consumer acceptance and installed development interfaces | In progress | libc remains partial; complete crt0/fcrt0/profiling headers/archives |

### Source inputs and independent terminals

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 83 | Manifest-driven build/install staging | To do | Per-component outputs, modes and provenance; replace hard-coded 11-program declarations |
| 84 | Scalable V7 image builder | To do | Current builder has 64 inodes; configurable inode table and adequate full-release capacity |
| 85 | Separate full IDE root from embedded bootstrap/test root | To do | Keep 512 KiB RAM baseline; full distribution must not inflate the kernel image |
| 86 | Per-component guest acceptance harness | To do | Disposable staged roots, serial fixtures and manifests; keep existing user disk intact |
| 87 | Historical source/data archive extraction | To do | Validate all six source archives and six lesson archives; no host-format assumptions |
| 88 | Resolve missing referenced support inputs | To do | Track bc lib.b and optional lex nrform/ebcform individually; recover or obtain explicit disposition |
| 89 | Separate per-terminal queues, settings and ownership | To do | Old 26; preserve COM1 regressions |
| 90 | COM2 hardware initialization and interrupt routing | To do | Old 26; second QEMU serial endpoint |
| 91 | Independent controlling terminals and per-TTY signals | To do | Old 26; cooked/raw, EOF, flow control and groups |
| 92 | Package ttyS1 and terminal configuration | To do | Old 26; console alias and /dev/tty routing |
| 93 | Two-terminal end-to-end acceptance | To do | Old 26; concurrent input/output and resource recovery |

### Multi-user startup and accounts

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 94 | Install native passwd/group/ttys/rc configuration | To do | Old 27; explicit account fixtures, homes, modes and ownership |
| 95 | Port stty | To do | Query/change each terminal independently |
| 96 | Port tty | To do | Controlling-terminal identification and no-TTY errors |
| 97 | Port mesg | To do | Terminal write-permission toggles |
| 98 | Port getty | To do | Line initialization, username handoff and respawn |
| 99 | Port login authentication and credentials | To do | Correct/wrong passwords, home, shell, UID/GID and environment |
| 100 | Integrate login/logout utmp and wtmp | To do | Records across concurrent sessions; native format documented |
| 101 | Complete multi-user init startup and respawn | To do | Existing init port extended; rc, ttys changes, logout and single-user recovery |
| 102 | Port passwd | To do | Password updates, authorization and recovery after failures |
| 103 | Port su | To do | Credential transition and environment modes |
| 104 | Port newgrp | To do | Group authorization and replacement shell |
| 105 | Port who | To do | Active users and session accounting |
| 106 | Port write | To do | Cross-terminal delivery and permission denials |
| 107 | Port wall | To do | Broadcast to logged-in users |
| 108 | Multi-user boot and reboot acceptance | To do | Old 27 completion; two concurrent logins, logout, init recovery, persistence |

### Additional libraries

| Step | Description | Status | Evidence / notes / acceptance |
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
| 118 | Complete Fortran runtime archive integration | To do | Actual native compiler consumers after backend steps |

### Essential commands and scripts

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 119 | Port basename | To do | Native command fixtures, failure statuses and installation |
| 120 | Port cal | To do | Native command fixtures, failure statuses and installation |
| 121 | Port chgrp | To do | Native command fixtures, failure statuses and installation |
| 122 | Port chmod | To do | Native command fixtures, failure statuses and installation |
| 123 | Port chown | To do | Native command fixtures, failure statuses and installation |
| 124 | Port expr | To do | Native command fixtures, failure statuses and installation |
| 125 | Port file | To do | Native ELF identification plus retained data classifications |
| 126 | Port kill | To do | Native command fixtures, failure statuses and installation |
| 127 | Port mkdir | To do | Privileged directory operations and failures |
| 128 | Port mv | To do | Native command fixtures, failure statuses and installation |
| 129 | Port nice | To do | Native command fixtures, failure statuses and installation |
| 130 | Port osh | To do | Retained alternate shell behavior |
| 131 | Port rmdir | To do | Privileged directory operations and failures |
| 132 | Port sleep | To do | Native command fixtures, failure statuses and installation |
| 133 | Port sync | To do | Native command fixtures, failure statuses and installation |
| 134 | Port test | To do | Native command fixtures, failure statuses and installation |
| 135 | Port yes | To do | Native command fixtures, failure statuses and installation |
| 136 | Install/test false script | To do | Original script behavior and dependencies |
| 137 | Install/test true script | To do | Original script behavior and dependencies |
| 138 | Install/test nohup script | To do | Original script behavior and dependencies |

### Text processing and editing

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 139 | Port cmp | To do | Source-defined behavior, byte fixtures and error cases |
| 140 | Port col | To do | Source-defined behavior, byte fixtures and error cases |
| 141 | Port comm | To do | Source-defined behavior, byte fixtures and error cases |
| 142 | Port diffh | To do | Source-defined behavior, byte fixtures and error cases |
| 143 | Port diff | To do | Source-defined behavior, byte fixtures and error cases |
| 144 | Port diff3 | To do | Private helper plus installed script |
| 145 | Port ed | To do | Editing, save/reopen and command/error transcripts |
| 146 | Port grep | To do | Source-defined behavior, byte fixtures and error cases |
| 147 | Port egrep | To do | Source-defined behavior, byte fixtures and error cases |
| 148 | Port fgrep | To do | Source-defined behavior, byte fixtures and error cases |
| 149 | Port find | To do | Traversal, predicates and command execution |
| 150 | Port join | To do | Source-defined behavior, byte fixtures and error cases |
| 151 | Port look | To do | Source-defined behavior, byte fixtures and error cases |
| 152 | Port od | To do | Source-defined behavior, byte fixtures and error cases |
| 153 | Port pr | To do | Source-defined behavior, byte fixtures and error cases |
| 154 | Port rev | To do | Source-defined behavior, byte fixtures and error cases |
| 155 | Port sed | To do | Source-defined behavior, byte fixtures and error cases |
| 156 | Port sort | To do | Temporary runs, merging and record ordering |
| 157 | Port sp | To do | Source-defined behavior, byte fixtures and error cases |
| 158 | Port split | To do | Source-defined behavior, byte fixtures and error cases |
| 159 | Port tabs | To do | Source-defined behavior, byte fixtures and error cases |
| 160 | Port tail | To do | Source-defined behavior, byte fixtures and error cases |
| 161 | Port tee | To do | Source-defined behavior, byte fixtures and error cases |
| 162 | Port tr | To do | Source-defined behavior, byte fixtures and error cases |
| 163 | Port tsort | To do | Source-defined behavior, byte fixtures and error cases |
| 164 | Port uniq | To do | Source-defined behavior, byte fixtures and error cases |
| 165 | Port wc | To do | Source-defined behavior, byte fixtures and error cases |

### Build generators and awk

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 166 | Port cb | To do | C-source formatting fixtures |
| 167 | Port m4 | To do | Macros, quoting, includes and recursion |
| 168 | Port make | To do | Dependencies, timestamps, recipes and failure handling |
| 169 | Port yacc grammar processing and tables | To do | Host-assisted bootstrap; grammar fixtures |
| 170 | Complete yacc skeleton and native parser generation | To do | Install yaccpar; generate, compile and run parsers |
| 171 | Port lex scanner generation | To do | C scanner skeleton and lexical fixtures |
| 172 | Port libln and lex/smallex runtime targets | To do | Generated scanner execution and support libraries |
| 173 | Complete lex/yacc native generator acceptance | To do | Bootstrap cycles resolved; optional skeleton gaps explicit |
| 174 | Port awk parser and generated helper tables | To do | Uses working lex/yacc; include proc/tokenscript |
| 175 | Port awk interpreter and data model | To do | Fields, arrays, arithmetic, strings and native widths |
| 176 | Complete awk I/O/functions and end-to-end acceptance | To do | Pipes, files, regex and complete program fixtures |

### Native C/Fortran toolchains

The target is ELF64 AMD64, SysV calling conventions, LP64 and static linking.
Host tools may bootstrap generated code; both supplied C compiler families
are accounted for. Final native rebuild acceptance is step 207.

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 177 | Freeze native object/toolchain ABI | To do | ELF64 AMD64, SysV calls, LP64 and static linking; host bootstrap allowed |
| 178 | Port cpp | To do | Preprocessing, include paths and generated-source fixtures |
| 179 | Port ar | To do | Native SysV/GNU archives plus explicit historical archive reading |
| 180 | Port arcv | To do | Historical archive conversion; no object-code translation |
| 181 | Port assembler parser, symbols and directives | To do | Rewrite supplied PDP-11 assembly in native code |
| 182 | Implement x86_64 instruction encoding | To do | Instructions required by native C/Fortran output |
| 183 | Implement assembler ELF relocatable output | To do | Sections, symbol tables and relocations |
| 184 | Complete assembler pass/helper integration | To do | as/as2 equivalent outputs and native assembly acceptance |
| 185 | Port linker symbol/archive resolution | To do | Retained ld logic adapted to ELF64 |
| 186 | Implement linker relocations and executable layout | To do | Static executable sections, entry, permissions and errors |
| 187 | Complete native linker acceptance | To do | Execute linked fixtures; reject malformed inputs |
| 188 | Port nm | To do | Native ELF/archive fixtures and toolchain integration |
| 189 | Port size | To do | Native ELF/archive fixtures and toolchain integration |
| 190 | Port strip | To do | Native ELF/archive fixtures and toolchain integration |
| 191 | Port ranlib | To do | Native ELF/archive fixtures and toolchain integration |
| 192 | Retarget shared mip compiler infrastructure | To do | Native types, IR, templates and supplied prototype helpers |
| 193 | Retarget PCC front end | To do | Native widths, declarations and shared mip integration |
| 194 | PCC integer/pointer/control-flow backend | To do | x86_64 code generation |
| 195 | PCC calls, structures and floating-point backend | To do | SysV AMD64 ABI and variadic mixed-register cases |
| 196 | Complete PCC driver and supplied alternate targets | To do | pcc script/ccom/fort; native program acceptance |
| 197 | Retarget original C c0 front end | To do | Native widths and pass format |
| 198 | Retarget original C c1 backend | To do | Integer/control/call/pointer generation |
| 199 | Retarget original C floating/aggregate generation | To do | Native ABI and compiler helper coverage |
| 200 | Retarget c2 optimizer and conversion helpers | To do | c2/fc1/cvopt targets; optimized/unoptimized comparison |
| 201 | Port cc driver | To do | Preprocessor/compiler/assembler/linker and diagnostics |
| 202 | Install/test lorder script | To do | Native symbol dependencies |
| 203 | Retarget f77 parser and semantic analysis | To do | f77pass1 and native symbol/type sizes |
| 204 | Retarget f77 integer/control/array backend | To do | Native frame, array and COMMON/EQUIVALENCE layouts |
| 205 | Retarget f77 real/complex/call backend | To do | Fortran ABI, hidden lengths and runtime calls |
| 206 | Complete f77 driver and runtime startup integration | To do | Compiled numerical and I/O programs |
| 207 | Native toolchain rebuild acceptance | To do | Rebuild supported V7 tools/userland inside Nexix; modern kernel/libc use host GCC |

### Mathematics and language tools

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 208 | Port dc | To do | Numeric/language fixtures and native execution |
| 209 | Port bc | To do | dc integration; optional lib.b unresolved until recovered/disposition |
| 210 | Port factor | To do | Numeric/language fixtures and native execution |
| 211 | Port primes | To do | Numeric/language fixtures and native execution |
| 212 | Port random | To do | Numeric/language fixtures and native execution |
| 213 | Port spline | To do | Numeric/language fixtures and native execution |
| 214 | Port units | To do | Install units data and numeric conversion fixtures |
| 215 | Port bas | To do | Supplied build is obsolete; restore required old library interfaces |
| 216 | Port ratfor | To do | Numeric/language fixtures and native execution |
| 217 | Port structure | To do | Retained restructuring engine |
| 218 | Port beautify and struct script | To do | libln dependency and complete wrapper behavior |

### Storage administration and archives

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 219 | Port mknod | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 220 | Port mount | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 221 | Port umount | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 222 | Port mkfs | To do | Format disposable media; mount and populate |
| 223 | Port df | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 224 | Port du | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 225 | Port dd | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 226 | Port sum | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 227 | Port clri | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 228 | Port dcheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 229 | Port icheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 230 | Port ncheck | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 231 | Port quot | To do | Disposable storage fixtures; bounds, permissions and error paths |
| 232 | Port dump | To do | Historical archive/media formats and full restore fixtures |
| 233 | Port dumpdir | To do | Historical archive/media formats and full restore fixtures |
| 234 | Port restor | To do | Historical archive/media formats and full restore fixtures |
| 235 | Port tar | To do | Both tar/v6tar; historical header fixtures and native round trips |
| 236 | Port tp | To do | Native tape equivalent and historical directory format |
| 237 | Port update | To do | Disposable storage fixtures; bounds, permissions and error paths |

### Jobs, mail and accounting

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 238 | Install native spool ownership and locking configuration | To do | Safe native fixtures, not historical user/mail contents |
| 239 | Port ac | To do | Source-defined service/accounting fixtures |
| 240 | Port accton | To do | Source-defined service/accounting fixtures |
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

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 251 | Port dmesg | To do | Diagnostics/status fixtures |
| 252 | Port iostat | To do | Native kernel-information interface; no historical structure offsets |
| 253 | Port ps | To do | Native kernel-information interface; no historical structure offsets |
| 254 | Port pstat | To do | Native kernel-information interface; no historical structure offsets |
| 255 | Port time | To do | Diagnostics/status fixtures |
| 256 | Port bs | To do | Diagnostics/status fixtures |
| 257 | Port prof | To do | Native symbols and profiling-record consumers |
| 258 | Port adb ELF/core/symbol reading | To do | Native executable/core interpretation |
| 259 | Port adb x86_64 instruction/register handling | To do | Disassembly and expression/address fixtures |
| 260 | Complete adb live debugging | To do | ptrace, stepping, breakpoints and memory changes |
| 261 | Port lint analysis passes | To do | lint1/lint2 and shared mip native types |
| 262 | Complete lint driver and description libraries | To do | Native header declarations and warning fixtures |

### Documents, manuals and learning

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 263 | Port roff | To do | Retained formatter behavior |
| 264 | Retarget nroff/troff parsing and layout core | To do | Native widths and formatter memory model |
| 265 | Regenerate native terminal/font tables | To do | mkfont/chars and supplied table targets |
| 266 | Complete nroff terminal output | To do | Readable serial text and formatter fixtures |
| 267 | Complete troff typesetter output | To do | Captured protocol and font/layout fixtures |
| 268 | Port eqn | To do | Equation parsing and troff integration |
| 269 | Port neqn | To do | Terminal equation formatting |
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
| 281 | Install formatter macros/parser descriptions/library data | To do | yaccpar, ncform, lint descriptions, tmac and shared inputs |
| 282 | Install public ASCII/Greek/equation data | To do | Byte fidelity and format consumers |
| 283 | Port learn engine | To do | Sessions, workspace isolation and helper execution |
| 284 | Port learn private tee/lcount helpers | To do | Distinct from /usr/bin/tee |
| 285 | Install/test lesson archives and learn workdirs | To do | All six archives and representative lesson sessions |
| 286 | Install all manual pages and native annotations | To do | 307 pages, sections 0–8 |
| 287 | Install/test original man script | To do | Section selection, -w, missing pages and formatter modes |
| 288 | Install/test technical documents and driving scripts | To do | Native formatter/tool references |

### Plotting and historical output

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 289 | Extract/adapt generic libplot | To do | Historical .c.a input, native plot records |
| 290 | Port Tektronix libplot backend | To do | libt4014 protocol fixtures |
| 291 | Port t300/t300s libplot backends | To do | Both terminal protocol variants |
| 292 | Port t450/vt0 libplot backends | To do | Archive/source-only targets and native endpoint equivalents |
| 293 | Port plot filters and script | To do | tek/t300/t300s/t450/vplot with wrapper |
| 294 | Port graph | To do | Plot/typesetter/printer protocol capture and consuming fixtures |
| 295 | Port tc | To do | Plot/typesetter/printer protocol capture and consuming fixtures |
| 296 | Port tk | To do | Plot/typesetter/printer protocol capture and consuming fixtures |
| 297 | Port vpr | To do | Plot/typesetter/printer protocol capture and consuming fixtures |

### Serial communications

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 298 | Port cu | To do | Two native serial endpoints and connection lifecycle |
| 299 | Port UUCP packet/connection and lock primitives | To do | Native serial link and spool ownership |
| 300 | Port uucp/uux submission | To do | Queue creation, paths and authorization |
| 301 | Port uucico transfer daemon | To do | Two-guest serial transfer and retry/disconnect handling |
| 302 | Port uuxqt execution daemon | To do | Remote queue processing and permissions |
| 303 | Port uulog/uuclean/uurecover | To do | Status, cleanup and interrupted-job recovery |
| 304 | Complete UUCP configuration/scripts and acceptance | To do | All supplied targets; serial transport only, no TCP/IP |

### Games with source and scripts

| Step | Description | Status | Evidence / notes / acceptance |
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

Each program has separate source/provenance recovery and implementation steps.
Historical binaries remain preserved; recovery or an approved alternative is
required before native-port acceptance.

| Step | Description | Status | Evidence / notes / acceptance |
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

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 344 | Native standalone runtime and libsa | To do | srt0, SYS/config/device I/O and isolated runtime |
| 345 | Standalone cat | To do | Boot/runtime file-reading fixture |
| 346 | Standalone mkfs | To do | Disposable-media formatting and native mount verification |
| 347 | Standalone icheck | To do | Known clean/corrupt filesystem fixtures |
| 348 | Standalone restor | To do | Restore into disposable media with standalone runtime |
| 349 | Native boot/mtboot equivalents | To do | Disk/tape startup paths and controlled QEMU acceptance |
| 350 | Port standalone maketape/build helpers | To do | cc+ld-stand/cpgroup and media creation |
| 351 | Account for historical boot/kernel binaries | To do | Preserve provenance; associate native outputs, never claim old binaries execute |

### Rebuild, upgrades and release acceptance

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 352 | Adapt/test in historical build script | To do | Use native paths/tools; all supported component recipes |
| 353 | Adapt/test cmake historical build script | To do | Use native paths/tools; all supported component recipes |
| 354 | Adapt/test makeall historical build script | To do | Use native paths/tools; all supported component recipes |
| 355 | Full native V7 source-tree build acceptance | To do | All supplied source components, helpers, libraries and explicit native equivalents |
| 356 | Package complete 0.1.0 distribution | To do | Programs, scripts, libraries, headers, source/build inputs, manuals/data and permissions |
| 357 | Persistent disk upgrade/install tooling | To do | Back up first; preserve accounts/data/TZ and timestamp disk formats |
| 358 | Full multi-user/time/storage/userland acceptance | To do | 16/128 MiB baseline; full IDE release at 128 MiB and two terminals |
| 359 | Close all component/source/device blockers | To do | All 217 entries/35 requirements traceable; no hidden omissions |
| 360 | Reproducible host-bootstrap build and release provenance | To do | Pin SDK inputs, artifact hashes, licensing and validation evidence |
| 361 | Finalize 0.1.0 release documentation and tag | To do | Only after all required leaves are implemented, tested and committed |

### Additional kernel gaps identified by the source audit

| Step | Description | Status | Evidence / notes / acceptance |
|---:|---|---|---|
| 362 | Complete V7 CPU usage and scheduler priority decay | To do | clock.c/slp.c: usage saturation, per-second decay, nice and wakeup priorities; deterministic scheduling tests |
| 363 | Complete subsecond ftime clock interface | To do | Atomic seconds/milliseconds snapshot and native ABI; V7 timezone fields, rollover and invalid-user-pointer tests |
| 364 | Implement native V7 kernel callout queue | To do | timeout callbacks, tick deadlines, ordering, bounded capacity and interrupt serialization; device consumers in steps 68–77 |

### Execution priorities and release acceptance

Step 37 records coverage relationships and validates the roadmap. Complete
staging, image and test infrastructure in steps 83–88 before expanding
installation substantially. Prioritize account/libc and set-ID prerequisites
needed for steps 89–108, delivering multi-user boot before the long userland tail.
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
installed manuals/data and resolution of explicit release blockers. All 217
components and 35 requirements must be traceable to tested native implementations,
assets or documented native equivalents. No required feature is silently removed
to declare the release complete.

`make TOOLCHAIN_PREFIX= test-inventory` checks full-archive coverage, byte fidelity
and inventory/roadmap consistency, including numbering, status totals and coverage
of every component, requirement and step. It does not prove native functional acceptance or
completion of the ports.

Version **0.2.0** adds PCI/e1000, lwIP IPv4/TCP/IP integration, sockets and
basic network utilities.

Start with single-core QEMU i440fx, serial console, and IDE storage. SMP,
physical hardware, UEFI, IPv6, secure remote access, and complete Nexix self-hosting follow
the first release. See [architecture notes](docs/architecture.md) and
[source provenance](reference/README.md).

`make TOOLCHAIN_PREFIX= v7-userland` builds V7 init, the Bourne shell and
nine utilities as native ELF64 executables. `make TOOLCHAIN_PREFIX= test-v7`
tests the rebuilt shell and utilities in QEMU with 16 MiB and 128 MiB RAM.
Normal boot now launches init as PID 1 and runs its single-user shell. See [V7 userland](docs/userland.md).

`make TOOLCHAIN_PREFIX= rootfs` packages the programs and startup files into
`build/root.img`. `make TOOLCHAIN_PREFIX= test-rootfs` validates the image on
the host and in QEMU. See [root image](docs/root-image.md). Root mounting and PID 1 startup are implemented; see [booting](docs/booting.md).

`make TOOLCHAIN_PREFIX= test-shell` verifies the running V7 shell through COM1,
including editing, signals, pipes, scripts, background commands and longer
sessions. See [interactive shell acceptance](docs/interactive-shell.md).

The boot RTC is interpreted as UTC; `make run` explicitly uses QEMU
`-rtc base=utc`. Successful initialization prints `Nexix: RTC UTC epoch=...`
and `NEXIX_RTC_OK`. Invalid or unavailable RTC data produces an explicit warning
and leaves the clock at the UNIX epoch plus elapsed ticks. Time continues from
the PIT after initialization; `stime` changes the system clock without writing
the RTC. Automatic V7 file timestamps and configurable timezone conversion are implemented.

Run `make TOOLCHAIN_PREFIX= test-rtc` to check RTC decoding and boot with a fixed
UTC leap-day date. Details are in [validation](docs/validation.md).

New files receive current access, modification and inode-change times. Try:

```sh
echo test > /tmp/test1.txt
ls -l /tmp/test1.txt
```

Use `make TOOLCHAIN_PREFIX= test-storage` to verify RTC-derived timestamps and
file changes survive three cold boots on disposable disks. Existing files with
zero timestamps keep those values until the corresponding file operation updates
them; new data writes set modification/change time, reads set access time.

Step 24 adds V7 `date` and `touch`. To test the new packaged commands with a
fresh persistent root while keeping the existing working disk:

```sh
make TOOLCHAIN_PREFIX= DISK_IMAGE=build/nexix-step24.img run
```

Inside Nexix:

```sh
date -u
TZ=CET-1CEST,M3.5.0,M10.5.0/3; export TZ
date
touch /tmp/touched
ls -l /tmp/touched
```

The default timezone is GMT; edit `rootfs/profile` to configure the default
for newly generated roots. Existing disk contents are preserved across builds
and do not automatically gain new executables or profile changes. See
[time and timezone support](docs/time.md) for supported TZ syntax and V7 options.

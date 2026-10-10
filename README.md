# Nexix

Current development version: **0.1.0**.

Nexix ports UNIX Seventh Edition (V7) to x86_64 and adds features including TCP/IP.
It preserves V7 source and behavior where practical, with native ELF64 programs
rebuilt from source rather than PDP-11 binary compatibility.

Current implementation: a freestanding Multiboot2 bootstrap enters long mode,
identity-maps the first GiB, initializes COM1, and validates the boot handoff.
It installs an IDT, reports fatal exceptions over serial, remaps the legacy PIC,
parses GRUB's memory map, allocates 4 KiB physical pages, and handles a roughly
100 Hz PIT timer. Kernel/low memory, boot data, modules, and firmware-reserved
ranges are excluded from allocation. Managed 4 KiB page tables protect kernel
code and read-only data, mark data non-executable, and leave page zero unmapped.
Separate address spaces support mapping, unmapping, permission changes, and
switching between independent user mappings. A 64-bit TSS supplies guarded kernel
entry stacks, with separate IST stacks for double faults and NMIs. Bootstrap test
code now executes in ring 3, receives timer interrupts, and returns to the kernel
through a test interrupt gate. Two contexts use distinct kernel entry stacks.
After the self-tests it prints `NEXIX_USER_MODE_OK`, `NEXIX_USER_STACK_OK`,
`NEXIX_USER_TIMER_OK`, `NEXIX_USER_RETURN_OK`, and `NEXIX_BOOT_OK`, then idles
with interrupts enabled.
V7 core structures use native pointers and sizes, with separate PDP-11 disk
layouts and tested byte encoders. Native process code now provides guarded
per-process stacks, context switching, timer preemption, sleep/wakeup, eager
user-memory fork, exit, wait, and orphan reparenting. Boot tests exercise kernel
tasks and a ring 3 parent/child pair, then reclaim their resources. This is a
native adaptation of V7 process behavior; rebuilt V7 userland does not run yet.
A native `int 0x80` syscall ABI now supports process lifecycle,
identity queries, tick accounting, umask, and yielding. User-copy helpers reject
invalid pointers and protect read-only pages. The bootstrap test gate uses
`int 0x81`; it remains separate from the supported native interface.
Native libc startup and a static library now run a C ELF64 program at CPL3,
with syscall wrappers, errno, environment access, string/memory functions,
and exit callbacks. See [libc foundation](docs/libc.md).
A synchronous block interface, RAM disk and V7 filesystem now support kernel
mount/lookup/read/write APIs, sparse files and chained block allocation. Boot
tests mount a small generated fixture and execute an ELF read from it. The
production exec catalog remains embedded until filesystem integration. See
[filesystem foundation](docs/filesystem.md).
File descriptors now support regular I/O, creation, truncation, links, stat,
cwd, dup, close-on-exec and blocking pipes, with reference ownership across
fork/exec/exit. See [file operations and pipes](docs/files.md).

## Build and boot

Dependencies: GNU Make, Python 3, an `x86_64-elf` GCC/binutils cross-toolchain,
GRUB's BIOS modules and `grub-mkrescue`, xorriso, and `qemu-system-x86_64`.
On Debian/Ubuntu the runtime tools are provided by `grub-common`, `grub-pc-bin`,
`xorriso`, and `qemu-system-x86`.

```sh
make check
make iso
make run
make smoke
make test
make test-process
make test-syscall
make userland
make test-libc
make test-fs
make test-files
```

For an initial build on an x86_64 Linux host with GCC/binutils, use
`make TOOLCHAIN_PREFIX= check` (and the same override for other targets).
This is a bootstrap convenience; a pinned cross-toolchain remains planned.
The kernel does not link host libc. Build artifacts go into `build/`.
`make smoke` boots one CPU with 128 MiB RAM under QEMU TCG, requires the serial
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
`make test-syscall` repeats the normal boot tests with 16 MiB RAM and saves
`build/syscall-16M.log`. Every successful boot must also print `NEXIX_SYSCALL_OK`
and the supporting ABI, user-copy, error, and fork markers. See the
[native syscall ABI](docs/syscall-abi.md) for register conventions, types, errors,
and supported calls. ELF64 loading and atomic exec replacement are implemented;
see [execution ABI](docs/exec.md). `make TOOLCHAIN_PREFIX= test-exec` runs the
ELF and fork/exec/wait tests with 16 MiB RAM (`build/exec-16M.log`). The current
program catalog includes native assembly and C fixtures. Libc startup and
initial wrappers are implemented; rebuilt V7 programs and the remaining libc
services are pending. `make TOOLCHAIN_PREFIX= test-libc` runs the C userland
tests with 16 MiB RAM (`build/libc-16M.log`).
`make TOOLCHAIN_PREFIX= test-files` exercises file operations, descriptor
ownership, permissions and blocking pipes with 16 MiB RAM (`build/files-16M.log`).

## Roadmap

Version **0.1.0** covers these 20 steps. Steps 1–13 are implemented; step 14
is next. Regression tests accompany implementation throughout the roadmap.

1. Import V7 sources and preserve licensing.
2. GRUB boot and x86_64 long mode.
3. Serial console, exceptions, and timer interrupts.
4. Memory-map parsing and physical-page allocation.
5. Paging and separate address spaces.
6. User mode, TSS, and guarded kernel stacks.
7. Adapt V7 types and preserve disk layouts.
8. Scheduling, sleep/wakeup, fork, exit, and wait.
9. Native syscall ABI and safe user-memory access.
10. ELF64 loading, exec, arguments, and environment.
11. Native libc startup and syscall wrappers.
12. Block interface, RAM root device, and V7 filesystem.
13. File descriptors, file operations, and pipes.
14. Serial TTY and terminal handling.
15. Signals and remaining init/shell syscalls.
16. Rebuild V7 init, shell, and basic utilities.
17. Generate a bootable root filesystem image.
18. Mount root and launch PID 1 and shell.
19. Verify interactive shell operation in QEMU.
20. Add persistent IDE-backed storage.

Version **0.2.0** adds PCI/e1000, lwIP IPv4/TCP/IP integration, sockets, and
basic network utilities.

Start with single-core QEMU i440fx, serial console, and IDE storage. SMP,
physical hardware, UEFI, IPv6, secure remote access, and self-hosting follow
the first release. See [architecture notes](docs/architecture.md) and
[source provenance](reference/README.md).

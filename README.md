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

Native libc supports the retained V7 init, shell and seven utilities. Signals,
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

Version **0.1.0** covers these 20 steps. Steps 1–20 are implemented. Regression tests accompany implementation throughout the roadmap.

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

`make TOOLCHAIN_PREFIX= v7-userland` builds V7 init, the Bourne shell and
seven utilities as native ELF64 executables. `make TOOLCHAIN_PREFIX= test-v7`
tests the rebuilt shell and utilities in QEMU with 16 MiB and 128 MiB RAM.
Normal boot now launches init as PID 1 and runs its single-user shell. See [V7 userland](docs/userland.md).

`make TOOLCHAIN_PREFIX= rootfs` packages the programs and startup files into
`build/root.img`. `make TOOLCHAIN_PREFIX= test-rootfs` validates the image on
the host and in QEMU. See [root image](docs/root-image.md). Root mounting and PID 1 startup are implemented; see [booting](docs/booting.md).

`make TOOLCHAIN_PREFIX= test-shell` verifies the running V7 shell through COM1,
including editing, signals, pipes, scripts, background commands and longer
sessions. See [interactive shell acceptance](docs/interactive-shell.md).

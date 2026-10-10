# Nexix

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
switching between independent user mappings. After memory/paging self-tests and ten timer interrupts it prints
`NEXIX_TIMER_OK` and `NEXIX_BOOT_OK`, then idles with interrupts enabled.
This foundation does not yet run V7 kernel code or userland.

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
```

For an initial build on an x86_64 Linux host with GCC/binutils, use
`make TOOLCHAIN_PREFIX= check` (and the same override for other targets).
This is a bootstrap convenience; a pinned cross-toolchain remains planned.
The kernel does not link host libc. Build artifacts go into `build/`.
`make smoke` boots one CPU with 128 MiB RAM under QEMU TCG, requires the serial
boot and timer markers within 15 seconds, and saves `build/boot.log`.
`make test-unit` runs host tests for boot-data parsing and the allocator.
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
`make test`. Paging tests run in kernel mode; user-mode entry is the next step.

## Roadmap

1. Reference-source import, build foundation, and reproducible toolchain.
2. Long-mode boot, exceptions, memory allocation, paging, and timer interrupts.
3. V7 processes, user address spaces, ELF64 execution, syscalls, and libc.
4. V7 filesystem, IDE storage, terminal, init, shell, and core utilities.
5. PCI/e1000, lwIP IPv4 integration, sockets, and basic network utilities.
6. Automated regression tests and an experimental release.

Start with single-core QEMU i440fx, serial console, and IDE storage. SMP,
physical hardware, UEFI, IPv6, secure remote access, and self-hosting follow
the first release. See [architecture notes](docs/architecture.md) and
[source provenance](reference/README.md).

# Initial V7 porting inventory

The reference distribution separates the kernel into `usr/sys/sys` (core logic),
`usr/sys/dev` (devices), `usr/sys/h` (headers), and `usr/sys/conf` (configuration
and PDP-11 assembly). Userland lives under `usr/src`.

First machine-dependent audit targets:

- Startup, trap entry, context switching, and user-copy assembly in kernel
  configuration and machine support: replace with x86_64 implementations.
- `machdep.c`, `ureg.c`, process layouts, and signal handling: replace PDP-11
  registers and address assumptions with the documented native ABI.
- `slp.c`, process syscalls, and execution: retain scheduling/process semantics
  while replacing segmentation, swapping assumptions, and a.out loading.
- `iget.c`, `fio.c`, `rdwri.c`, and block-device support: retain filesystem logic,
  audit on-disk widths and byte ordering, and introduce RAM/IDE block devices.
- Historical libc assembly, startup, varargs, and syscall wrappers: implement
  native AMD64 versions before compiling init, shell, and utilities.

The reference import, bootstrap build, and an observed QEMU boot are implemented.
IDT exception diagnostics and PIC/PIT timer interrupts are implemented as the
first machine-support increment. Bounded Multiboot2 memory-map parsing and a
physical-page allocator provide the next increment. Managed 4 KiB page tables,
kernel protection, and independent address spaces are implemented. CPL3 test
execution, TSS kernel-stack switching, guarded entry/IST stacks, and user
protection checks are also implemented. V7 process/types are adapted, and native
scheduling, wait-channel sleep/wakeup, eager fork, exit/wait, and reparenting are
implemented and exercised in QEMU. The behavior follows `slp.c` and `sys1.c`;
PDP-11 assembly, segmentation, and swapping are replaced rather than compiled.
Native syscall dispatch now follows V7's call numbering for its initial supported
services, with a defined x86_64 register ABI and validated user-memory access.
File/inode reference ownership, signals, ELF execution, and remaining services
are pending; see [syscall-abi.md](syscall-abi.md).
A pinned cross-toolchain remains required to
close the foundation milestone.
This inventory identifies entry points, not a completed source compatibility audit.

# Stability investigation

The reported intermittent VGA/serial freeze is still under investigation.
Passing bounded acceptance runs do not establish that it has been fixed.

Run `make test-stability` for 100 rounds on each of the 16 and 128 MiB
configurations, or `make test-stability STABILITY_ROUNDS=1000` for a longer run.
Each round checks hostname queries, a pipeline through two children, and
filesystem reads/writes. Two independent logins stay active, and periodic idle
intervals exercise scheduler halt and UART wakeup. Tests create disposable disks;
they never modify `build/nexix.img`. Serial and QEMU logs are retained in
`build/stability-<RAM>.tty0.log`, `.tty1.log` and `.qemu.log`, including on failure.
Use `make test-vga` for actual PS/2 input and VGA display acceptance.

The full boot self-test installs temporary native executables and writes core
images. It now extends its private RAM volume to 1 MiB before these tests;
the shipped recovery image and ordinary recovery filesystem remain 512 KiB.
The block-device boundary tests read their expected size from the superblock.
This addresses test scratch-space exhaustion, rather than reducing the checks.

For an unrecoverable lockup, record the exact launch command or libvirt XML,
RAM/CPU count, emulator and accelerator, attached console devices, last command,
and whether the guest was idle. Nexix currently targets one CPU, BIOS/i440fx,
IDE storage, and VGA/PS/2 or an explicitly selected serial console. A QEMU monitor
`info registers` and `info pic` snapshot taken while frozen can distinguish a
CPU fault, an interrupt-disabled spin, and an idle CPU waiting for an interrupt.

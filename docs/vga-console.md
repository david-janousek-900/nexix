# VGA console in virt-manager

Fresh full installations provide three independent logins: `/dev/console`
(COM1, also `/dev/ttyS0`), `/dev/ttyS1` (COM2), and `/dev/tty0` (VGA display
and PS/2 keyboard, character device major 1, minor 2). `/dev/tty` follows the
current process's controlling terminal. Boot and panic diagnostics use COM1.

Build with `make build`. In virt-manager, create an x86_64 VM using BIOS
firmware and an i440fx/PC chipset. Attach `build/nexix.iso` as a bootable CD-ROM
and the full Nexix disk as an IDE disk. Select **VGA** video and a graphical
SPICE or VNC display; use the emulated PS/2 keyboard. Start with 128 MiB RAM
and one CPU. The display backend does not change the guest's VGA/PS/2 drivers.
UEFI, virtio-only video and USB-only keyboards are not supported by this driver.
Do not attach a working disk to two running VMs at once.

Open the VM's graphical console and log in as `root` with password `nexix`,
`alice`/`alice`, or `bob`/`bob` on a fresh development installation. Passwords
are hidden. Backspace and Ctrl-U edit input; Ctrl-C interrupts a foreground
program; Ctrl-D logs out. The keyboard layout is US. There is one 80×25 text
screen with scrolling and a hardware cursor; no scrollback, virtual terminal
switching, or ANSI cursor-addressing emulation is provided.

`make run-vga` starts the same hardware with QEMU's GTK display and COM1 on
the launching host terminal. COM2 remains accessible through `make terminal`.
`make run` retains its serial-only host display. Both targets preserve an
existing `DISK_IMAGE`; QEMU must include GTK support for `make run-vga`.

## Existing installations

Rebuilding the ISO updates the kernel but does not modify installed devices
or `/etc/ttys` on an existing disk. Enabling the local login requires a
root-owned mode-0600 character node `/dev/tty0` with major 1/minor 2 and an
additional `19tty0` line in `/etc/ttys`, preserving the serial entries. Reboot
after the configuration change. The current installed utilities do not include
`mknod`, so an older image without this node cannot be upgraded using its shell
alone; automatic disk upgrades remain task 357.

For an immediately usable graphical login without changing the old image,
stop the VM and run `make DISK_IMAGE=build/vga-fresh.img run-vga` with an unused
image path. This seeds a new full installation; the previous disk and its
accounts/files remain intact. In virt-manager, likewise use a copy of the new
full distribution seed for a fresh installation, retaining the old disk.
The embedded RAM recovery root retains its existing single-user serial setup.

## Validation

`make test-vga-unit test-keyboard-unit test-tty-lines-unit` exercises display,
keyboard and independent TTY behavior on the host. `make test-vga` boots
production-seed copies with standard VGA and PS/2 at 16 and 128 MiB, injects
keys through QMP, and observes actual VGA memory. It checks login, hidden
passwords, editing, terminal identity, signals, logout/relogin and simultaneous
serial sessions. Screen and serial logs are saved under `build/vga-*`.
Tests preserve the working disk. Local QMP sockets must be permitted.

ISO kernels use deterministic gzip compression, transparently decoded by GRUB,
to avoid loader memory exhaustion at 16 MiB. The uncompressed `build/nexix.elf`
remains available for debugging and layout checks.

For manual virt-manager acceptance, confirm a visible login, enter a username
with a Backspace correction, verify hidden password input, run `tty` (expect
`/dev/tty0`) and `echo VGA_OK`, interrupt `cat` with Ctrl-C, then log out with
Ctrl-D and log in again. Confirm serial input and output remain independent.

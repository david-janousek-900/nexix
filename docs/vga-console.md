# VGA console in virt-manager

The default console is **VGA/PS/2**. Kernel boot messages, panic diagnostics,
`/dev/console`, startup-script output and the login session all use the selected
console. The other transport remains silent. Console selection is a Multiboot2
kernel option, independent of the disk image:

| Mode | Run command | ISO / boot option |
| --- | --- | --- |
| VGA (default) | `make run` or `make run-vga` | `build/nexix.iso`, `console=vga` |
| COM1 serial | `make run-serial` | `build/nexix-serial.iso`, `console=serial` |
| Independent VGA, COM1 and COM2 | See below | `build/nexix-multi.iso`, `console=multi` |

`make CONSOLE=serial run` is an alternative to `make run-serial`.
`make serial-iso` builds the serial ISO without launching QEMU. Use that ISO in
virt-manager for serial-only operation; use the default ISO for its graphical
console. Missing `console=` defaults to VGA. Invalid or duplicate selections
are rejected with a VGA handoff diagnostic. Valid options may be combined with
test options in either order.

The default installation retains `/dev/console` and `/dev/ttyS0` as historical
hard-link aliases (major 1/minor 0); both follow the selected console. In VGA mode
that device resolves to the same terminal as `/dev/tty0` (major 1/minor 2).
`/dev/tty` follows the process's controlling terminal. Init receives the selected
mode from the kernel and starts only the `console` entry in `/etc/ttys` for the
two exclusive modes, avoiding duplicate gettys on the same terminal. COM2 is
inactive in those modes. Console selection requires an installed init built with
this support; rebuilding the ISO alone does not update old installed programs.

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

`make run` and `make run-vga` use QEMU's GTK display. `make run-serial` uses
the launching host terminal; Ctrl-A, release, then X exits serial QEMU.
All run targets preserve an existing `DISK_IMAGE`. VGA requires QEMU GTK support.
Kernel diagnostics have uptime timestamps; root can read them later with
`dmesg`. See [kernel log and root-home changes](kernel-log.md).

## Independent terminals

`console=multi` retains the earlier three-terminal configuration for applications
and regression tests that explicitly need independent sessions: COM1 diagnostics
and `/dev/console`, COM2 `/dev/ttyS1`, and VGA/PS/2 `/dev/tty0`.
Build it with `make multi-iso`. Attach `build/nexix-multi.iso` to the same BIOS/IDE
VM instead of the default ISO, with two UARTs for both serial logins.
The serial acceptance harness and `make test-vga` use this explicit profile;
`make test-console` verifies the exclusive production profiles. `make terminal`
attaches COM2 only when the launching QEMU has configured its `build/ttyS1` pipes
and the guest is booted in multi mode.

## Existing installations

Rebuilding the ISO updates the kernel but does not update init or configuration
on an existing disk. Use a freshly seeded disk for the exclusive console policy,
or install the rebuilt `/etc/init` yourself. Keep an enabled `19console` entry in
`/etc/ttys`. The kernel passes the console mode to init; the same disk can then
boot either ISO without editing `/etc/ttys`.

For the optional multi profile, local login requires a root-owned mode-0600
`/dev/tty0` character node (major 1/minor 2) and a `19tty0` entry alongside the
serial entries. The currently installed utilities cannot create missing device
nodes; automatic disk upgrades remain task 357.

For an immediately usable graphical login without changing the old image,
stop the VM and run `make DISK_IMAGE=build/vga-fresh.img run-vga` with an unused
image path. This seeds a new full installation; the previous disk and its
accounts/files remain intact. In virt-manager, likewise use a copy of the new
full distribution seed for a fresh installation, retaining the old disk.
The embedded RAM recovery root retains single-user startup and follows the
selected console too.

## Validation

`make test-console` checks exclusive VGA/serial routing at 16 and 128 MiB,
including boot messages, rc, login, shell/device writes, logout and a real
NMI-triggered panic. It also checks that inactive UARTs/screens receive no Nexix
output. Evidence is saved in `build/console-*`.

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
`/dev/console` in default mode, `/dev/tty0` in multi mode) and `echo VGA_OK`, interrupt `cat` with Ctrl-C, then log out with
Ctrl-D and log in again. Confirm the serial terminals remain silent in default VGA mode.

## Acceptance evidence

Tasks 367–371 were validated on 2026-10-19:

- `make test-vga` passed at 16 and 128 MiB, including displayed username
  Backspace correction, password hiding, shell Backspace/Ctrl-U, Ctrl-C,
  Ctrl-D/relogin, three terminal identities and independent serial sessions.
- `make test-inventory test-rootfs test-guest-harness-unit test-tty test-getty
  test-login test-multiuser-acceptance` passed; guest checks cover both memory
  sizes. `make test-two-terminals` passed binary UART traffic, signal/flow
  isolation, concurrent shells and repeated resource recovery at both sizes.
- A transient `qemu:///session` libvirt guest used BIOS, PC/i440fx, IDE,
  standard VGA, PS/2 and VNC with a disposable production-seed disk. Libvirt
  key injection exercised edited username, hidden password, `/dev/tty0`, shell
  output, Ctrl-C and logout/relogin. The actual virt-manager graphical console
  was captured and visually inspected. The temporary VM was destroyed afterward.

Local evidence artifacts are `build/vga-acceptance.log`,
`build/vga-regressions.log`, `build/vga-serial-regressions.log`,
`build/vga-getty-regression.log` and `build/virt-manager-vga.png`.

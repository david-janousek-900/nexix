# Nexix

Nexix is a work in progress port of UNIX Seventh Edition (V7) to native x86_64
hardware. Its objective is to bring the V7 kernel behavior, commands, libraries,
development tools and documentation to a modern architecture while preserving
historical source and behavior where practical. Programs are rebuilt as native
ELF64 executables; Nexix does not emulate PDP-11 binaries.

The current development version is **0.1.0**. It boots in QEMU to multi-user login
on a VGA/PS/2 console by default, with optional serial mode. It provides persistent
IDE storage, the V7 Bourne shell and 72 native ELF programs. The kernel provides protected process
address spaces, scheduling, fork/exec, signals, pipes, permissions and filesystem
operations. Native libc, libm, startup objects and headers support the ported
programs. The complete V7 port and guest self-hosting are still unfinished.
TCP/IP networking is planned for **0.2.0**; it is not currently available.

Traditional BSD `vi`, `ex` and read-only `view` are installed alongside V7 `ed`.
See [vi usage, recovery and tests](docs/vi-port.md). The `clear` command clears
the VGA or ANSI serial terminal.

The `man` command reads the V7 manuals with a navigable VGA/serial pager.
Use Space/b for pages, j/k or arrows for lines, /text to search and q to quit.
See [manuals and formatter modes](docs/man-pages.md). The native
[ps](docs/ps.md) command lists processes; [uptime](docs/uptime.md) reports
elapsed boot time, logged-in sessions and 1/5/15-minute scheduler load.

The [reference review](docs/reference-review.md) records the latest coverage
audit, including command compatibility tasks 389–408 and startup integration
task 409. These remain planned work, separate from completed initial ports.

See the [task list](docs/tasks.md) for completed and remaining work, the
[installation manifest](docs/native-install.json) for installed programs, and the
[porting inventory](docs/porting-inventory.md) for historical source coverage.
See [text utilities](docs/text-commands.md) and [V7 make](docs/make.md) for
native command behavior and limits. [uname](docs/uname.md) reports the running
kernel's name, release, version and architecture. [Hostname configuration](docs/hostname.md)
uses `/etc/hostname` and supplies `username@hostname` login prompts.

## Supported environment

The supported development host is **x86_64 Linux** with GCC and GNU binutils.
The initial guest platform is QEMU's BIOS PC/i440fx machine, one CPU, an IDE disk,
standard VGA and a PS/2 keyboard. Start with **128 MiB RAM**. Automated guest tests
also exercise selected features at 16 MiB. UEFI, SMP and physical hardware are
outside the current supported baseline.

## Prerequisites

Install these tools on the host, not inside Nexix:

| Tool | Purpose |
| --- | --- |
| Git | Obtain the repository and its historical source inputs. |
| GNU Make | Run the build recipes and test targets. |
| GCC, GNU binutils and C development tools | Compile and link the kernel, native programs and host tests. |
| Python 3 | Generate filesystem images and run validation tools; no pip packages are required. |
| GRUB tools and BIOS modules | Put the kernel and a BIOS bootloader on a bootable ISO. |
| xorriso | Create the ISO through `grub-mkrescue`. |
| QEMU x86 system emulator | Boot Nexix and run guest acceptance tests. |
| QEMU GTK display support (optional) | Open the local VGA console with `make run-vga`. |
| virt-manager and libvirt (optional) | Manage a graphical VM instead of launching QEMU through Make. |

For Debian/Ubuntu:

```sh
sudo apt update
sudo apt install git build-essential binutils python3 grub-common grub-pc-bin xorriso qemu-system-x86
```

The first command refreshes package metadata; the second installs the compiler,
image-building tools and emulator. For the graphical QEMU console, also install:

```sh
sudo apt install qemu-system-gui
```

Other distributions use different package names. Ensure `gcc`, `ld`, `make`,
`python3`, `grub-mkrescue`, `xorriso` and `qemu-system-x86_64` are on `PATH` and
GRUB's BIOS modules are installed. The default emulator uses software acceleration
(TCG), so KVM access and root privileges are not required to build or run Nexix.

## Build

1. Clone the repository and enter its root directory:

   ```sh
   git clone https://github.com/david-janousek-900/nexix.git
   cd nexix
   ```

   The checkout includes the pinned historical reference inputs. The normal
   build does not download a separate V7 distribution or external dependencies.
   Run the remaining commands from this directory.

2. Build the kernel, boot ISO and persistent working disk:

   ```sh
   make build
   ```

   Make compiles native code, checks the kernel's ELF layout, stages the guest
   installation, builds a V7 filesystem image and packages the kernel with GRUB.
   On the first build it seeds `build/nexix.img` from the full distribution image.
   Later builds preserve that working disk and its files. The default compiler
   prefix is empty; `TOOLCHAIN_PREFIX=` can explicitly select host GCC/binutils.

3. Check the build artifacts and boot a short self-test:

   ```sh
   make check
   make smoke
   ```

   `check` validates the kernel ELF layout. `smoke` boots a dedicated self-test
   kernel in QEMU at 128 MiB and checks its expected results. Neither command
   constitutes the full test suite.

| Artifact | Meaning |
| --- | --- |
| `build/nexix.elf` | Uncompressed kernel ELF with symbols for inspection/debugging. |
| `build/nexix.iso` | Bootable GRUB ISO containing the compressed kernel. |
| `build/distribution.img` | Generated full guest installation seed. |
| `build/nexix.img` | Persistent working IDE disk used by default. |
| `build/root.img` | Small embedded recovery filesystem, not the full installation. |
| `build/stage/` | Staged installation files and installation receipt. |

`make stage`, `make rootfs` and `make iso` build the staging tree, full filesystem
seed and boot ISO separately. See [image construction](docs/root-images.md) and
[installation staging](docs/install-staging.md) for their contracts.

## Run and log in

For the VGA console, run:

```sh
make run
```

This builds any missing artifacts and opens QEMU's GTK window with standard VGA
and a PS/2 keyboard. Wait for `login:` and use `root` with password `nexix`.
Fresh development images also provide `alice`/`alice` and `bob`/`bob`. Password
input is hidden. These are development defaults, not deployment credentials.
The local keyboard layout is US. Root starts in its private `/root` directory;
Backspace or Ctrl-H removes the previous input character.

For a host-terminal serial console, including on a headless host, run:

```sh
make run-serial
```

This connects COM1 (`/dev/console`, also `/dev/ttyS0`) to the launching terminal.
In both modes `/dev/console` follows the selected device, including kernel boot
and panic messages, startup scripts and login. The inactive console stays silent.
`make run-vga` explicitly selects VGA; `make CONSOLE=serial run` is equivalent
to `make run-serial`, which builds and boots `build/nexix-serial.iso` for COM1. Both modes use the same persistent disk.
An optional multi-terminal profile is described in the [console guide](docs/vga-console.md).

After logging in, try:

```sh
who
pwd
ls -l /
echo hello > /tmp/hello
cat /tmp/hello
date -u
dmesg
ps ax
uptime
man ls
sync
```

These commands show active sessions, inspect the filesystem, create and read a
file, display UTC time, read timestamped kernel messages and flush filesystem
updates. `dmesg` is available to root; it does not clear the log. Ctrl-D logs out;
Ctrl-C interrupts a foreground command. To stop serial QEMU, press Ctrl-A,
release it, then press X. For VGA QEMU, close its window after flushing updates.

The working disk persists between boots. Rebuilding does **not** install new
programs or configuration into an existing disk. For an offline upgrade, stop
QEMU, run `make upgrade-preview`, then `make UPGRADE_BACKUP=build/before-upgrade.img upgrade-disk`.
This backs up the old image, updates managed programs, and preserves accounts,
existing configuration and user files. See [disk upgrades](docs/disk-upgrades.md).
To try a fresh installation
while preserving the old one, choose an unused image filename:

```sh
make DISK_IMAGE=build/fresh.img run-vga
```

Make seeds that disk only if it does not already exist. The ISO must remain
attached for booting: the current disk image does not contain an installed
bootloader. **`make clean` deletes the entire `build/` directory, including the
default persistent disk.** Copy disks elsewhere before cleaning if you need them.

For virt-manager, create a BIOS x86_64 PC/i440fx VM with one CPU and 128 MiB RAM,
attach `build/nexix.iso` as the bootable CD-ROM and `build/nexix.img` as an IDE
disk, and select VGA video with a SPICE or VNC display and PS/2 keyboard. Open
the graphical console to log in. Stop any Make-launched VM before reusing its
disk. See the [VGA/virt-manager guide](docs/vga-console.md) for detailed setup,
existing-image migration and console limitations.

For live investigation of freezes, use `make run-debug` (serial) or
`make DEBUG=1 run` (VGA). Leave the guest running and ask for analysis;
`make debug-capture` from another host terminal saves registers and RAM.
See [live debugging](docs/live-debugging.md) for details.

## Test

```sh
make test-inventory
make test-man
make test-pager
make test-ps
make test-uptime
make test-yes-interrupt
make test-unit
make test-essential
make test-vga
make test-console
make test-dmesg
```

`test-yes-interrupt` runs `yes` for one minute on each serial terminal, then
checks Ctrl-C, shell recovery, a repeated interruption and the other login at
16 and 128 MiB. It uses disposable disks and continuously drains output. For a
longer run (duration in seconds per terminal):

```sh
python3 tools/yes_interrupt_smoke.py qemu-system-x86_64 build/nexix-multi.iso 128M 300
```

`test-inventory` checks historical file hashes, source coverage and task mappings.
`test-unit` runs host-side checks, including compiler sanitizer tests.
`test-essential` exercises the essential command ports in guests at 16 and
128 MiB. `test-vga` drives real emulated VGA/PS/2 login and shell sessions at
both memory sizes. `test-console` checks exclusive VGA/serial routing, including
boot, login, `/dev/console` writes and panic output. `test-dmesg` checks retained
messages, incremental reads and permissions. Guest tests use disposable images rather than the working
disk; VGA tests require local QMP sockets. Run `make test` for the broader
regression suite. Some tests intentionally provoke kernel faults and accept
only the expected diagnostic. See [validation](docs/validation.md) for details.

## Repository guide

- [Architecture](docs/architecture.md), [boot sequence](docs/booting.md) and [storage](docs/storage.md)
- [Userland](docs/userland.md), [multi-user sessions](docs/multi-user.md) and [time/timezones](docs/time.md)
- [Kernel messages and dmesg](docs/kernel-log.md)
- [Development interfaces](docs/development-interfaces.md) and [syscall ABI](docs/syscall-abi.md)
- [Tasks and release scope](docs/tasks.md) and [task coverage rules](docs/roadmap-coverage.md)
- [Historical source provenance](reference/README.md) and [supplemental inputs](docs/support-inputs.md)

Active ports belong outside `reference/v7/`; keep the historical distribution
unchanged. Task completion requires implementation, acceptance tests and a
commit, as described in the task list.

## Licensing

Original Nexix contributions are licensed under the **BSD 3-Clause License** in
[LICENSE](LICENSE). Historical UNIX code and derived portions retain the
[Caldera license](LICENSES/Caldera.txt), including its advertising acknowledgement.
The imported Ratfor lex skeleton retains CDDL-1.0. These files are not relicensed
by the BSD license. See [third-party notices and the license audit](THIRD_PARTY_NOTICES.md)
for scope, archived-document exceptions and boot-ISO redistribution requirements.

This product includes software developed or owned by Caldera International, Inc.

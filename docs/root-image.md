# Packaged root filesystem (task 17)

`make bootstrap-rootfs` creates `build/root.img`, a deterministic
1024-sector (512 KiB) V7 filesystem volume. It uses the retained PDP-11 disk
encodings and native x86_64 ELF64 user programs. The kernel/GRUB boot loader
lives on the ISO; this volume supplies userland and has no independent boot
sector. `make TOOLCHAIN_PREFIX= iso` embeds the root image in the kernel.

This describes the fixed embedded recovery image. `make rootfs` now builds the
separate full `build/distribution.img`, with COM2 and owned user homes/account
configuration; see [installation staging](install-staging.md) and
[multi-user configuration](multi-user.md). Existing working disks are preserved.

The bootstrap image includes:

- `/etc/init` and `/bin/sh`.
- `/bin/echo`, `cat`, `ls`, `pwd`, `cp`, `ln`, `rm`, `date`, and `touch`.
- `/etc/rc`, `/etc/ttys`, `/etc/passwd`, `/etc/hostname`, `/etc/profile`, and `/.profile` from `rootfs/bootstrap/`.
- Empty `/etc/utmp` and `/usr/adm/wtmp` accounting files.
- `/dev/console` and `/dev/ttyS0`, two names for the same COM1 device inode,
  plus `/dev/tty` for the controlling terminal and `/dev/null` for EOF/discard I/O.
- `/dev/root` and `/dev/rroot` expose block/raw reads of the current root backing
  device; see [storage device contracts](storage-devices.md). Mounted root writes
  through these device nodes return `EBUSY`. Existing disk images are preserved.
- `/bin`, `/etc`, `/dev`, `/tmp`, `/usr`, and `/usr/adm` directories.

All files belong to root. Directories/programs have mode 0755, configuration
and accounting files 0644, terminal devices 0600, and `/dev/null` 0666. Directory link counts,
hard links, free blocks and free inodes are initialized. File timestamps are
zero so identical source binaries and configuration produce identical bytes.
The root profile exports PATH=/bin and HOME=/root and sets the
[identity prompt](hostname.md). The passwd file describes root;
login and getty are excluded from this recovery image. `/etc/ttys` is empty to disable getty
entries; initial operation will use init's single-user shell.

`tools/mk_rootfs.py` reuses the disk encoder from `tools/mk_v7fs.py` while
keeping the older regression volume (`build/root-test.img`) separate.
It rejects programs without native ELF headers and volumes too small for the
payload. Its `--blocks`, `--programs`, `--config` and `--output` options support
standalone generation. No loop devices, root privileges or host filesystem
mounts are needed.

`make TOOLCHAIN_PREFIX= test-root-image` checks packaged file bytes, device
aliases, directory links, free-stack chains, block/inode accounting,
reproducibility, invalid-program rejection and undersized-volume rejection on the host.
`make TOOLCHAIN_PREFIX= test-rootfs` adds QEMU tests with 16 MiB and 128 MiB RAM.
Nexix temporarily mounts the embedded production volume read-only, checks the
layout and modes, reads every executable through V7 directory/indirect-block
APIs, verifies its bytes and loads it into a separate address space. All
loader pages are reclaimed and the temporary mount is released. Required
markers are `ROOT IMAGE OK` and `ROOT ELF OK`; logs are
`build/rootfs-16M.log` and `build/rootfs-128M.log`.

Normal boot mounts an IDE working copy of the volume, or a RAM copy when
no primary master is attached, and launches init and
the single-user shell; see [booting.md](booting.md).
Interactive acceptance and persistent IDE storage complete tasks 19 and 20.

Init loads `/etc/hostname` before recovery starts. The `hostname` command itself
is installed in the full IDE distribution, alongside other non-bootstrap tools.

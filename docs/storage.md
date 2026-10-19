# Persistent root storage (task 20)

`make run` boots the ISO and attaches `build/nexix.img` as QEMU's primary IDE
master. `make disk` seeds a new working disk from `build/distribution.img` using
atomic, exclusive publication. Existing disks are preserved byte-for-byte,
including when their contents differ from the current seed. Files and metadata
therefore survive restarts. `make clean` deletes the entire `build/` directory,
including working disks and saved files.

The ISO supplies GRUB and the kernel; the raw disk contains an unpartitioned V7
filesystem. The fixed 512 KiB `build/root.img` is embedded for RAM bootstrap and
tests. It is separate from the full IDE distribution and is used only when no
primary IDE root device is found. An invalid attached IDE root fails explicitly.
See [root image profiles](root-images.md).

To test persistence yourself:

```sh
make run
# Inside Nexix:
echo hello > /tmp/message
cat /tmp/message
# Quit QEMU with Ctrl-A, release, then X.
make run
# Inside Nexix:
cat /tmp/message
```

To boot the latest packaged installation without changing the existing working
disk, choose a new path:

```sh
make rootfs
make DISK_IMAGE=build/fresh.img run
```

Rebuilding userland updates the distribution seed, not an existing working disk.
Existing small disks remain small and retain their installed programs; migration
and upgrade tooling belongs to task 357. Copy a working disk while QEMU is stopped
to back it up.

The driver identifies the primary master, requires LBA28, 512-byte sectors and
cache-flush support, and transfers one sector at a time through polling PIO.
Every successful write includes an ATA cache flush. Polling has a finite limit;
device faults, unsupported geometry and flush failures return I/O errors.
The filesystem already writes through its buffer cache and marks a mounted
volume failed after a write error. This provides persistence for completed
writes; filesystem operations have no journal or crash-atomic transaction.
DMA, IDE interrupts, additional drives, partitions, AHCI and NVMe are outside
this initial driver.

Normal boot prints `ROOT IDE` for the IDE root. If the primary master is
absent, it prints `ROOT RAM` and uses the embedded root. An attached drive
that fails identification or mounting causes a boot panic instead of silently
starting another root. Diskless boot/shell tests and RAM filesystem fixtures
remain available.

`make TOOLCHAIN_PREFIX= test-storage` uses disposable disks at 16 MiB and
128 MiB, never the working disk. Each test starts QEMU three times and checks
persistent file contents, hard links, append/unlink, copied ELF execution,
single-indirect file blocks, reuse after freeing blocks, restored free-block/inode
totals and rejection of an attached invalid root. Transcripts are
`build/storage-16M.log` and `build/storage-128M.log`. Host driver tests use port
emulation to check absent devices, bounded busy timeouts, unsupported geometry,
cache-flush requirements, unaligned multi-sector buffers, bounds, read errors
and flush failures. These checks are part of `make test`.

The emulator command and identify-bit definitions can be checked against the
[QEMU IDE implementation](https://github.com/qemu/qemu/blob/master/hw/ide/core.c).

`DISK_IMAGE` selects the working image (default `build/nexix.img`). For example,
`make TOOLCHAIN_PREFIX= DISK_IMAGE=build/nexix-task24.img run` creates a separate
root with the latest packaged programs if that path does not exist. The disk
rule never replaces an existing image. Rebuilding updates the root seed and
kernel, not the files on an existing persistent root.

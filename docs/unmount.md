# Unmount and busy-volume handling (task 55)

The reference implementation is sys3.c (sumount/getmdev), text.c
(xumount/xuntext), alloc.c (update), bio.c (bflush), iget.c (iput), fio.c
(file references/device close), sys4.c (cwd/root references) and sys1.c
(process exit). The libc umount.s wrapper is replaced by a native syscall
wrapper. The reference command remains task 221, separate from kernel/ABI/libc.

## Interface and V7 behavior

`umount(block_device_path)` is syscall 22, declared in `<unistd.h>`. The device
is selected by its device number, so another block-special pathname with the
same rdev works. The lookup reference is not retained during the busy scan:
a device node inside the volume being unmounted can name that volume.

Nexix requires effective UID 0, consistent with its mount/storage-administration
policy. This is an explicit native restriction: the supplied V7 smount/sumount
routines do not themselves call suser. Path copy/search and device-type checks
remain applicable. Non-block paths return ENOTBLK, invalid configured major/minor
ranges ENXIO, absent paths ENOENT, invalid user pointers EFAULT, and a valid
block device with no mounted volume EINVAL. The permanently held global root
returns EBUSY.

As in V7, unmount purges unused sticky text for the requested device, invokes
global filesystem update, and then checks active inode references. Open files,
open-unlinked files, directory descriptors, process cwd/root references and
active executable text return EBUSY. Dup/fork references remain busy until
all owning descriptions/process references are released. Text on other devices
is preserved; active text on the requested device is never discarded.

Nested mounts and volumes covering a mounted root keep their parent volume
busy through their mount-point pins. Their children must be unmounted first.
An open block/raw device node on another filesystem does not itself count as
an active inode on the target volume, matching V7's inode/device scan. Existing
storage handles remain valid after the mount's own handle closes. Writable raw
backings become writable again after successful unmount; physically read-only
backings still return EROFS.

## Synchronization and removal

The existing Nexix filesystem is write-through and synchronous; IDE sector
writes include device-cache flushes. Global sync visits all mounted volumes,
updates writable superblock time and attempts every volume even if an earlier
one fails. Read-only volumes are not written.

Native error contract: any sync error, including an error on another volume,
returns EIO and retains the requested mount, its cache, pin and device-write
guard. Busy checking follows successful synchronization. A sticky-text cleanup
or mount-point preflight error also prevents removal. Sticky cleanup/global
updates may already have happened on a rejected unmount, as in V7's ordering;
this is not an all-or-nothing filesystem transaction.

Before commit, the mount-point inode is checked for its sole reference and a
positive disk link count. Mounted-on entries cannot be unlinked. Because inode
metadata is already written through, dropping that linked final pin requires
no further disk I/O. The commit clears the pin/IMOUNT state, volume cache and
mount table entry, then closes the mount's storage handle. Current storage
callbacks have no fallible close operation; there is no disk work after commit.
A later remount reads fresh metadata/data from the backing device.

A write error still marks its filesystem failed under the existing fail-stop
policy. Retaining a failed mount does not make its contents usable or provide
an automatic recovery path. Fault tests repair only their injected superblock
failure through private active-test gates; no production repair API is added.

Individual removals create holes in the mount table. Kernel-only whole-namespace
teardown now removes leaves of the parent-child graph rather than assuming slot
numbers reflect mount order. This handles a child reusing a lower-numbered slot
than its parent. It retains existing reference checks and never removes the
production global root through the user syscall.

## Validation

`make test-umount` runs host unmount and selective-text tests, native QEMU
root/syscall tests at 16/128 MiB, inventory/coverage checks, isolated ABI headers
and libc export checks. Native fixtures cover descriptor/dup/fork ownership,
open-unlinked files, cwd/root holders, active and inactive sticky text on two
volumes, nested/overlaid mounts, aliases, an internal device pathname, invalid
calls, remount persistence, injected target/unrelated sync failures, read-only
unmounts and slot reuse. Host fixtures additionally verify cache invalidation
through raw backing changes, mount-point preflight failure retention, all-volume
sync attempts and graph teardown after slot reuse. Exact root inode/block/page
counts and storage handles are checked after native cleanup. Test disks are
disposable; build/nexix.img is preserved.

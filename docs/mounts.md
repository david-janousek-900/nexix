# Multi-volume namespace and mount (task 54)

Reference behavior comes from sys3.c (smount/getmdev), iget.c (device/inode
identity and mount crossing), nami.c (namei and mounted-root dotdot), alloc.c
(getfs/update), sys4.c (chroot/chdir), sys2.c (cross-device link rejection),
main.c (root mount), h/mount.h, h/inode.h and h/param.h. Libc mount.s is replaced
by a native syscall wrapper; reference sources remain unchanged.

## Interface and ownership

`mount(device_path, mountpoint_path, read_only)` is syscall 21, exported by
Nexix libc in `<unistd.h>`. Effective UID 0 is required. The device pathname
must name a configured block device. The flag's low bit selects read-only.
The private table has eight volumes including the global root, matching V7
NMOUNT. Each secondary mount owns its filesystem/cache, a storage handle and
one reference to the underlying mount-point inode, marked IMOUNT. The global
root remains device 0; secondary identities use the configured block device
number. No disk inode format or published stat record size changes.

Every cached inode is keyed by device plus inode number. stat/fstat expose that
identity through st_dev/st_ino. File I/O, metadata, inode reclamation, cwd/root
references, executable text and core dumps use the inode's volume. Separate
volumes may safely contain identical inode numbers. Dup/fork still share
open-file descriptions; cwd/root references are inherited and released on exit.

V7 permits ordinary files as mount points as well as directories. Referenced
mount points, duplicate device mounts and exhausted mount tables return EBUSY.
An unreferenced mounted root can itself be covered by another volume, as in
V7. The global root is permanently reserved, equivalent to V7's held rootdir
reference. Mounting the same backing block_device through a second registered
alias is rejected. Invalid device majors/minors return ENXIO; unbound valid
slots return ENODEV; non-block device paths return ENOTBLK.

Superblock/root validation completes before publication. Failure releases the
driver handle, mount-point reference and slot. Writes to writable backing
devices through block/raw aliases, including descriptors opened before mounting,
return EBUSY while mounted. Physically read-only backings retain EROFS. This retains task 53's explicit cache-protection adaptation.

## Lookup and filesystem operations

Path traversal crosses mount points into the mounted root. At a mounted root,
`..` traverses through the underlying mount-point directory to its parent,
including nested mounts and multiple volumes covering a mounted root. As in
V7, `..` through a mount on an ordinary file returns ENOTDIR. Search permissions,
14-byte name truncation and trailing-slash directory checks remain in effect.
Unlinking an underlying mount-point entry returns EBUSY.

Absolute paths start at the process root. `..` at that device/inode identity
stays there, including paths that climb out of nested mounts. chroot keeps V7's
behavior: it changes the process root but does not move its cwd or revoke
existing descriptors. A cwd outside the new root remains usable through
relative paths; this is not a containment extension.

Creation and metadata changes use the resolved parent/file volume, effective
credentials, umask and that volume's read-only state. Cross-volume hard links
return EXDEV; links within a volume retain existing V7 behavior. sync visits
all mounted volumes, reports the first error and still attempts the others.

## Scope and validation

User umount/busy-volume policy is implemented by [task 55](unmount.md).
Kernel-only quiescent teardown checks references and removes leaves of the
mount graph, so slot reuse cannot release a parent before its child. The reference mount command is task 220; its umount command is task 221.
The current production driver still registers only the RAM/IDE root. Kernel
storage_bind enables additional backings; discovery/registration of additional
physical disks is a subsequent driver task. Native tests register disposable
RAM volumes and never alter build/nexix.img.

`make test-mount` runs host mount/lookup/failure fixtures and native QEMU tests
at 16 and 128 MiB, plus source inventory, ABI headers and libc export checks.
Tests cover nested and overlaid mounts, dotdot/chroot, permissions, read-only
mounts, malformed/device-error rollback, all seven secondary slots, cross-volume
links, equal inode numbers, mounted exec/core output, descriptor and cwd lifetime,
covered-file restoration and exact root page/block/inode cleanup. The native
fixture builds directories with V7 mknod plus privileged dot/dotdot links.
Full smoke runs have a 45-second bound because they execute all cumulative
kernel/libc/root fixtures; fault-only smoke runs retain the shorter bound.

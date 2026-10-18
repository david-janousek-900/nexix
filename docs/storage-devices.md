# Block and raw storage devices (task 53)

V7 sources: sys/sys2.c, sys/rdwri.c, sys/fio.c, dev/bio.c (physio),
conf/c.c and h/conf.h. Historical driver entry points call physio; their
controller-specific replacements remain task 70.

Nexix uses the V7 16-bit device encoding: high byte major, low byte minor.
Block major 0 and raw character major 4 share eight storage slots. Minor 0
is the current root backing device, either RAM or IDE. Fresh root images
include /dev/root (block 0x0000) and /dev/rroot (character 0x0400), mode 0600.
Existing persistent images are preserved; root can create these nodes with
mknod using the same mode/device values. No new mknod command is installed.
Other slots are available through the kernel storage_bind interface.

Block I/O accepts byte offsets and lengths and uses sector read/modify/write
for partial writes. Special inode size stays zero; SEEK_END uses that inode
size, as in V7. O_APPEND does not reposition device I/O. Dup and fork share
file offsets and a single driver handle, released on the final close.
Unconfigured major/minor pairs return ENODEV; storage ioctls return ENOTTY.
Ordinary inode permissions and syscall user-range checks apply.

Raw I/O preserves physio's even user-address/even-count checks (EFAULT),
replacing PDP-11 segmentation/DMA with bounded synchronous kernel bounce
buffers. As in V7 physio, raw transfers start at the sector containing the file
offset (offset >> 9); the descriptor offset advances by the byte count moved.
Mapped native user pages replace the historical data/stack segment restriction.
Zero-length transfers return zero after normal descriptor/range validation.
The kernel does not DMA into user memory or require user pages to be contiguous.

At device end reads return EOF; beyond the end they return EIO. Writes at or
beyond the end return EIO. A transfer that reaches the boundary or encounters
an error after moving bytes returns its partial byte count and advances its
shared offset by exactly that count. Read-only backing devices return EROFS.

Native safety adaptation: writes to any mounted backing device return
EBUSY, including through aliases/raw nodes, to prevent bypassing the filesystem
cache and superblock state. Unmounted registered devices allow writes. This
is stricter than V7 and is explicit; mounted-volume cache coordination remains pending;
multi-volume mounts are implemented by [task 54](mounts.md) and user unmount
by [task 55](unmount.md). Binding replacement or detaching a slot with open handles returns EBUSY. Calls are serialized by
the single-CPU kernel with IRQs disabled; backing lifetime must cover binding
and all open handles.

Validation: make test-storage runs injected host device failures, bounds and
read/modify/write checks, independent root image checks, and native QEMU
filesystem/syscall tests at 16 and 128 MiB, followed by disposable IDE
persistence checks at both memory sizes. Native fixtures use separate
three-sector writable/read-only devices; production persistent storage is
never modified by these tests.

Task 70 adds [native V7 disk-controller equivalents](disk-controllers.md),
including partition-aware mounted-alias protection and auxiliary ATA media.
Run `make test-disk` for host models and 16/128 MiB cold-boot persistence.

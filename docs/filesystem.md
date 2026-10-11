# Block devices and V7 filesystem foundation (step 12)

Nexix has a synchronous block-device interface, RAM-backed storage and native
V7 filesystem APIs. The algorithms follow the retained V7 alloc.c, subr.c bmap,
iget.c, rdwri.c and nami.c sources. Historical disk bytes are decoded explicitly;
PDP-11 source assembly and native host struct layouts are not used for storage.

## Build and test

```
make TOOLCHAIN_PREFIX= test-unit
make TOOLCHAIN_PREFIX= test-fs
make -j3 TOOLCHAIN_PREFIX= test
```

The build generates `build/root-test.img`, a 1024-block (512 KiB) V7 filesystem
fixture, and embeds its immutable bytes in the kernel. The boot test copies it
into writable RAM, mounts it temporarily, tests it, then unmounts it. This is the
RAM root-device foundation. Normal boot now mounts the production image and
launches init; see [booting.md](booting.md). Step 17 now builds the init/shell root image; see [root-image.md](root-image.md).
Step 18 mounts it for PID 1. The fixture itself is not a bootable V7 userland distribution.

The fixture contains root, bin, tmp and dev directories; a greeting file;
patterned data spanning direct, single and double indirection; a sparse file
using triple indirection; a scratch file; a 14-byte filename; native ELF test
programs; and character-device inodes. Step 14 connects console/ttyS0 and
controlling-tty names to COM1 through the file layer; see [tty.md](tty.md).
`tools/mk_v7fs.py` builds this deterministic image from the generated executables.
It writes PDP-11 short/long/three-byte address encodings independently of the
kernel's C helpers. Host tests pin representative bytes of the generated image.

## Block API and cache

`kernel/include/block.h` defines a device size, read-only flag, context, and
single-sector read/write callbacks. Public transfers validate the complete
sector range before invoking callbacks. RAM reads and writes copy bytes from
caller-owned backing storage and reject misaligned image lengths. Buffers are
trusted kernel pointers. Callback failures return negative Nexix errno; a
multi-sector generic transfer can have completed an earlier prefix.

The filesystem uses a 29-entry sector cache. Reads fill it on demand with round
robin replacement; failed reads never publish cache entries. Writes go to the
device synchronously and update any cached copy. There is no delayed-write
queue, DMA, interrupt-driven driver or buffer sleep/wakeup yet. Step 20 adds a polling IDE driver; see [storage.md](storage.md).

Callers serialize complete filesystem operations; current kernel callers keep
interrupts disabled. Backing memory, device callbacks and device flags must
remain stable while mounted. Callers use one filesystem instance per device;
there is no registry enforcing exclusive mounts yet. Directly changing device
bytes behind the cache
is unsupported. The block-device/backing memory belongs to the caller, and the
filesystem owns no physical allocator pages.

## Filesystem API

`kernel/include/v7/fs.h` provides mount/unmount, inode inspection, pathname
lookup, file reads/writes, and low-level block allocation/freeing. APIs take
kernel pathnames and buffers and report partial I/O through a `done` count.
They return zero on success or a negative Nexix errno on failure. File offsets
are signed 64-bit values; negative offsets are rejected.

Mount validates volume/inode-list bounds, free-stack counts and entries, cached
free-inode numbers, and root inode type/size. Inode reads validate regular-file
sizes, directory alignment, and inode block addresses. Indirect pointers are
validated before use. This is bounded format validation, not a full filesystem
consistency scan or fsck. There is no format magic in the V7 superblock.

Storage layout is unchanged:

- Block 0 is reserved; block 1 holds the superblock.
- Blocks 2 through s_isize-1 hold eight 64-byte inodes each; root is inode 2.
- Directory records hold a 16-bit inode and a fixed 14-byte name.
- Inodes hold ten direct addresses plus single/double/triple indirect roots.
- Indirect blocks hold 128 PDP-11-format 32-bit addresses.
- The free list is a stack of up to 50 addresses, replenished from chained free
  blocks with a 16-bit count followed by PDP-11-format 32-bit addresses.

Reads support regular files and directories, EOF and zero-filled sparse holes.
These storage helpers return ENODEV for special inode I/O; the file layer
dispatches supported character devices to the terminal instead.
Writes support existing regular files, partial sectors and sparse growth,
allocating zeroed data/indirect blocks as needed. An old allocated sector tail
is cleared before exposing a gap beyond EOF. The maximum file length is
1,082,201,088 bytes, determined by V7's address tree. Disk inode addresses remain
24-bit, and mounted inode numbers remain 16-bit.

Absolute lookup begins at inode 2; relative lookup uses the supplied cwd inode.
Repeated slashes, dot and dot-dot use directory entries. A trailing slash
requires a directory. Following V7 nami.c, components longer than 14 bytes are
truncated for comparison, rather than assigned a modern long-name format.
There are no symlinks, nested mount points or chroot handling in this kernel-only
API. The step 13 file layer uses credential-checked lookup and checks access
before exposing storage operations as syscalls; see
[files.md](files.md).

Allocation consumes the on-disk V7 free stack, zeroes returned blocks, and
writes metadata synchronously. Freeing rebuilds chained stacks. The generator
and these operations maintain s_tfree as a count hint; compatibility with older
V7 images does not depend on that historically unmaintained field. Low-level
freeing requires exclusive caller ownership. Invalid ranges and duplicates in
the current stack are rejected; arbitrary duplicate frees across older chain
blocks are not detected. This API is for the file-management layer, not users.

## Errors and remaining work

Bad metadata/pointers return EIO; exhausted free blocks return ENOSPC; oversized
writes return EFBIG; invalid offsets/buffers return EINVAL. Read-only mounts
return EROFS before writing. Writes to directory inodes return EISDIR.
Failed reads can be retried. Any device write failure marks the mount failed,
so subsequent APIs return EIO until unmount/remount. Remount invalidates caches;
it does not repair damaged disk metadata. Writes are not journaled or crash
atomic. Allocation may leave owned empty indirect branches after ENOSPC, which
can be reused by a subsequent write. Power-loss recovery is not implemented.

Step 13 implements file-descriptor operations, inode/file reference ownership,
permission checks, namespace creation/removal, truncation and pipes. Step 15 adds explicit metadata updates, step 18 adds filesystem-backed exec,
and step 23 maintains automatic V7 timestamps.
Normal boot runs programs from the production root. The embedded catalog is
retained for regression fixtures.

## Validation

Every successful QEMU boot requires `NEXIX_BLOCK_OK`, `NEXIX_V7FS_READ_OK`,
`NEXIX_V7FS_ALLOC_OK`, `NEXIX_V7FS_WRITE_OK`, `NEXIX_V7FS_EXEC_OK` and
`NEXIX_V7FS_OK` before `NEXIX_BOOT_OK`. `test-fs` uses 16 MiB RAM and logs to
`build/fs-16M.log`; smoke uses 128 MiB. The full suite uses test-fs as its
low-memory regression boot; the earlier test-process/syscall/exec/libc targets
remain available separately.

Boot tests cover paths, directories, special inodes, EOF, all indirection
levels, sparse reads/writes, free-list exhaustion and rebuilding, allocation
zeroing, read-only rejection and write persistence across cache-invalidating
remounts. A native C ELF program is read from /bin/libc-test and executed at
CPL3; its process and memory resources are reclaimed.

The host filesystem tests add malformed superblocks, root inodes, directory
references, inode and indirect addresses, chained free lists, range overflow,
read/write device faults, cache retry/hits, partial-transfer counts and stale
bytes past EOF, and recovery from ENOSPC during indirect construction. They run against a copy of the image and leave the build image
unchanged. They also pass with address/undefined-behavior sanitizers enabled.

## V7 timestamp handling (step 23)

The event rules follow the preserved V7 `rdwri.c`, `iget.c`, `sys4.c` and
`nami.c`: new inodes receive atime/mtime/ctime, nonempty reads update atime
(including EOF), completed write chunks update mtime/ctime, and truncation
updates mtime/ctime. Metadata and link-count changes update ctime. Directory
entry writes update the parent directory's mtime/ctime, while pathname scans
do not change directory atime. Explicit utime values replace atime/mtime;
ctime always comes from the system clock. Character-device reads/writes mark
their persistent inode; pipes have no persistent inode. Zero-count regular-file
reads/writes do not change times.

V7 inode times remain four bytes each in the existing PDP-11 encoding, with
second precision and no birth-time field. Automatic updates store the low
32 bits of epoch seconds, preserving the existing disk width and rollover.
No new timestamp fields or timezone offsets are stored. Timezone conversion
belongs in libc (step 24). Read-only mounts skip timestamp writes.

The native filesystem retains its synchronous write-through policy: timestamps
are stored during the operation rather than deferred through V7 inode flags
and the buffer daemon. Repeated access in the same second avoids unnecessary
inode writes. A timestamp write failure follows the existing mount-poisoning
policy; a completed partial transfer can still return its byte count. `sync`
also records current V7 superblock time on writable mounts. Crash atomicity
and power-loss recovery remain outside this milestone.

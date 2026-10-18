# Task 84: scalable V7 image construction

`tools/mk_v7fs.py` implements the V7 disk format; `tools/mk_rootfs.py` places the
native installation manifest into it. The primary reference is
`reference/v7/usr/src/cmd/mkfs.c`, together with `sys/{filsys,ino,fblk,dir,param}.h`,
`usr/sys/sys/alloc.c` and `iget.c`. The original guest mkfs port remains task 222.
The preserved reference sources and V7 timestamp/disk encodings are unchanged.

## Sizing

The default `make rootfs` still produces the byte-identical 512 KiB bootstrap
root with 96 inode slots. Task 85 owns separating the full IDE distribution from
this embedded bootstrap; task 84 does not enlarge the kernel's RAM root.

To create a separate image from the verified installation stage:

```
make stage
python3 tools/mk_rootfs.py --stage build/stage --blocks auto --output /tmp/nexix-root.img
python3 tools/mk_rootfs.py --stage build/stage --blocks 131072 --inodes 4096 --output /tmp/nexix-large.img
```

Sizes are in 512-byte blocks. `--blocks auto` counts file and directory data,
all single/double/triple indirect blocks, and inode/superblock overhead. It adds
25% headroom or 128 free blocks, whichever is greater, with a minimum volume of
256 blocks. `--spare-blocks N` replaces that default automatic free-space budget;
for an explicit volume size it requires at least N blocks to remain free.

Without `--inodes`, the manifest's highest assigned inode plus at least 24 free
slots determines the inode area, rounded up to blocks of eight and retaining a
12-block minimum for bootstrap compatibility. `--spare-inodes N` changes this
automatic budget. Hard-link aliases do not consume additional inodes. Explicit
`--inodes N` selects capacity rounded up to eight slots, must fit all installed
objects and replaces the automatic spare-inode budget.

The artificial 32 MiB volume and 1,000-inode limits are removed. Bounds follow
the native V7 filesystem decoder: at most 16,777,216 blocks (8 GiB) and 65,528
inode slots (8,191 inode blocks). Inode 1 is reserved and root is inode 2. The
original mkfs used a slightly more conservative 65,496-slot sizing cap; the
native maximum follows the unsigned 16-bit inode number and eight-slot block
layout. Files retain ten direct and three indirect roots with 128 addresses per
indirect block. Single-file capacity is `(10+128+128²+128³)*512` bytes, not the
whole volume. Inode link counts retain the 32,767 limit.

## Construction and failure handling

Large volumes use sparse temporary-file-backed mappings rather than allocating
a volume-sized Python bytearray. Free-list construction iterates the unused
allocation suffix; it no longer creates and sorts a set containing every volume
block. The free-inode cache contains at most 100 entries while the superblock
records the total free inode count. Directory ordering and hard-link counting
use indexed structures and a stable topological order, preserving old output
bytes without repeatedly scanning the whole manifest for each entry.

The root builder checks capacity and free-space budgets before image allocation.
The low-level builder validates inode-table bounds, sectors, sparse extents,
directory names/targets, file addressing and metadata widths. Oversized files
and impossible layouts fail instead of extending the inode table into data.

CLI output is written to a temporary sibling, flushed and atomically renamed
only after construction succeeds. Failed sizing, output writes or publication
preserve an existing output. Symlink output destinations are refused. The upper
format bounds do not promise that the host has enough temporary/output disk
space; payloads are still materialized from the staging plan. The publication
step writes a complete image, so provision host space accordingly. These tools
do not overwrite `build/nexix.img` unless explicitly directed to that path.

## Acceptance

`make test-image-builder` runs independent byte-level ownership/free-list checks,
a production-filesystem host test and QEMU acceptance at 16/128 MiB. The disposable
64 MiB fixture has 4,096 inode slots, 1,600 data files, a 34 MiB filler file and a
hard link. Its late files require inode numbers above 1,000 and physical block
addresses above 65,535. The host mounts it, creates more than 100 inodes, verifies
remount persistence and reclamation, then exhausts the complete chained free
list. V7 directory blocks retain empty slots after unlink; this is accounted for.

Two QEMU cold boots verify high-inode/high-block reads, hard links, copying and
persistent data through real COM1. Other tests cover automatic inode growth,
exact free-space budgets, indirect boundaries, maximum format bounds without
allocating an 8 GiB image, invalid layouts, symlink outputs and failed atomic
publication. Existing unit, shell and persistent-storage regressions run before
commit; the working disk is preserved.

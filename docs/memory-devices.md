# Native memory and special devices (task 69)

The original implementation is `usr/sys/dev/mem.c` (`mmread`, `mmwrite`)
and `usr/sys/dev/sys.c` (`syopen`, `syread`, `sywrite`, `sysioctl`). Device
registration comes from `conf/c.c` and `conf/mkconf.c`, with dispatch contracts
in `h/conf.h`, `sys/fio.c` and `sys/rdwri.c`. Manuals `mem(4)`, `null(4)` and
`tty(4)` describe their behavior. Reference inputs remain unchanged.

`kernel/v7/memdev.c` implements byte-addressed physical/kernel memory and the
null sink. `kernel/v7/files.c` dispatches by the special inode's device number,
so aliases created by mknod use the same implementation. Existing TTY open,
read/write and ioctl paths supply the controlling-terminal equivalent from
`sys.c`; no PDP-11 segmentation registers or private user-area layout are used.

## Nodes and device numbers

Fresh packaged roots contain:

| Node | Character device | Mode | Meaning |
|---|---|---|---|
| `/dev/mem` | 6:0 (0x0600) | 0600 | Physical byte addresses |
| `/dev/kmem` | 6:1 (0x0601) | 0600 | Kernel virtual byte addresses |
| `/dev/null` | 3:0 (0x0300) | 0666 | Discard writes; reads return EOF |
| `/dev/tty` | 2:0 (0x0200) | 0666 | Calling process's controlling terminal |

Major 6 minor 2 (0x0602) is also the null device, retaining V7 mem.c's
0/1/2 minor meanings. Existing Nexix null device 3:0 remains supported.
Unknown major-6 minors fail ENXIO. Constants live in `<nexix/memdev.h>`;
existing device encodings and ABI version remain unchanged.

Persistent working disks are preserved and do not automatically acquire new
nodes or the updated public `/dev/tty` mode. A fresh disk can be selected with `make DISK_IMAGE=build/nexix-task69.img run`.
Existing native programs can create the nodes using mknod(2); the standalone
mknod command remains with its own port task.

## Native access rules

Memory-device open and every read/write require effective UID zero, even if
filesystem mode bits are relaxed or a descriptor is inherited before dropping
privilege. These checks use the V7 suser/ASU accounting path. Normal inode
permissions still apply. Null and controlling-terminal devices have no added
root requirement; ordinary node permissions apply.

Offsets are byte addresses. Mem uses physical addresses with validated kernel
identity aliases; kmem translates kernel virtual addresses through the kernel
page tables. The current native kernel region is below 1 GiB, with identity
aliases, so many mem/kmem offsets coincide. User virtual addresses are not kernel
virtual addresses and cannot be supplied to kmem to select another process.

A present identity PTE is insufficient proof of backing: the whole range must
refer to allocator-eligible RAM, the loaded kernel image/BSS, or the existing
VGA text aperture (0xb8000–0xbffff). This includes free and allocated managed RAM
for privileged memory examination/patching. Null pages, guard pages, reserved
firmware ranges, ROM, RAM holes and other MMIO are rejected with ENXIO. VGA
aliases retain the paging layer's uncached attributes. Future device apertures
need explicit backing/cache-policy integration.

Reads require present supervisor mappings. Writes additionally require writable
page-table leaves; direct writes to kernel text/rodata return EACCES. Kernel
writable data and managed RAM can be examined and patched, preserving mem(4)'s
applicable capability. This privileged debugging interface is separate from
phys(2): user-accessible phys mappings still cannot claim kernel/ordinary process
frames. No new user mapping or allocation is created by memory-device I/O.

Both the user buffer and every memory page are preflighted before transferring
bytes. Crossing a hole or read-only leaf fails the entire operation without
advancing the file offset or changing memory/the user buffer. Transfers are
serialized on the single CPU, never sleep or allocate, and use volatile byte
accesses with bounded user-copy buffers. This is stricter than V7's possible
partial transfer before a bad address. Invalid user buffers return EFAULT;
negative or overflowing offsets return EINVAL; missing backing returns ENXIO.
Zero-length transfers return zero after descriptor, privilege and range-contract
checks, without dereferencing either address.

## File and terminal behavior

Dup and fork share the open-file offset; successful memory transfers advance it
by exactly the transferred byte count. Seek uses byte offsets, with SEEK_END
relative to the special inode's zero size. O_APPEND/O_TRUNC do not reinterpret
memory addresses or resize devices. Memory/null ioctls return ENOTTY except for
the existing generic FIOCLEX/FIONCLEX descriptor operations. Stat retains the
character inode mode/device and zero size. Existing V7 character-device inode
timestamp handling applies.

Null writes discard the requested bytes and advance the shared offset; reads
return zero without advancing it. Existing native full-user-buffer validation
is retained even though the sink does not copy those bytes. Offset overflow is
rejected before accepting a write. Major 6 minor 2 and legacy 3:0 behave alike.

Opening `/dev/tty` without a controlling terminal returns ENXIO. With one, it
retains the underlying terminal and forwards read/write/ioctl through the same
line discipline as `/dev/console`, including raw/cooked and packet modes.
Close/dup/fork/exec use the established TTY/open-file lifetime. COM1 remains the
only physical terminal; independent TTY routing remains tasks 89–93.

Kernel-information file records add kind `NX_KINFO_FILE_MEMORY` (6) for these
memory/null special files. Record sizes, field offsets and snapshot version
remain unchanged; no kernel pointers are added to snapshots. Diagnostic command
ports should continue using the explicit kinfo contracts for portable tables.

## Validation

Run `make test-memdev`. Host tests use real production driver code with controlled
page-table/backing fixtures: byte and cross-page transfers, nonidentity kmem
translation, permission/backing preflight, bounds/holes, root checks and both
null device encodings. Header and root-image tests check native device constants,
node modes and encodings, including public access to the controlling-TTY node.

Native user tests run through actual syscalls during the root acceptance boot
at 16 and 128 MiB. They examine and patch a writable kernel fixture through
mem/kmem, verify it independently in the kernel, reject rodata writes and invalid
addresses, check cross-page physical aliases, shared dup/fork offsets, inherited
and newly opened descriptors after dropping privilege, generic ioctl behavior,
null aliases and unknown minors. They also exercise absent controlling-terminal
open, non-root controlling-TTY open/ioctl, and TTY read/write/ioctl forwarding. Root cleanup verifies no inode, file,
physical-frame or allocator-page leaks. No new user executable is installed.

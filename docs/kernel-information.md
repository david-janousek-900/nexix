# Kernel-information snapshots (task 62)

V7 `ps.c`, `pstat.c` and `iostat.c` find `/unix` symbols and read private tables
through `/dev/mem`, `/dev/kmem` and, historically, swap. Their source contracts
come from `usr/sys/h/{proc,user,file,inode,text,mount,map,buf,systm}.h` and the
process, inode, file, mount, allocator, buffer-I/O and clock implementations.
Nexix replaces those layout-dependent reads with syscall 72, `NX_SYS_KINFO`.
No PDP-11 structure overlays or private kernel pointers are exported.

Include `<sys/kinfo.h>` and call:

```
ssize_t bytes = nx_kinfo(NX_KINFO_VERSION, NX_KINFO_PROCESS, NULL, 0);
/* Check errors, allocate bytes, then: */
ssize_t used = nx_kinfo(NX_KINFO_VERSION, NX_KINFO_PROCESS, buffer, bytes);
```

`nx_kinfo(unsigned version, unsigned kind, void *buffer, size_t capacity)` returns
the required/written byte count, or -1/errno. PROCESS and LOAD are public,
address-free snapshots used by `ps` and `uptime`. FILE, INODE, TEXT, MOUNT,
MEMORY and IO require effective UID zero for sizing and data calls (EPERM
otherwise). Authorized privileged calls mark ASU for accounting. The libc wrapper
does not grant access to raw memory.

Version 1 defines eight kinds: PROCESS, FILE, INODE, TEXT, MOUNT, MEMORY, IO
and the native LOAD extension.
A null buffer with capacity zero queries size. A non-null short buffer returns
ERANGE without writing; size can change between calls as processes/files/mounts
change, so callers must re-query and retry. A null buffer with nonzero capacity
returns EFAULT. Invalid version/kind, including high bits on raw syscall arguments,
returns EINVAL before permission checking. Sufficient buffers are checked for
writable user mappings over the entire required range before publication; invalid,
read-only, wrapped or partly unmapped ranges return EFAULT with no partial writes.
Unused capacity is untouched. Normal user-pointer lifetime rules apply.

Each capture counts, validates and serializes with interrupts disabled on the
single CPU, without allocation, disk I/O or scheduling. Tables cannot change
between count and publication. Each kind is coherent within its call; separate
kinds/calls are not one transaction. A size query does not reserve a snapshot.
The kernel uses a single 144-byte stack record, not a shared large staging buffer.

## Version 1 records

All records use fixed-width native little-endian fields. The shared contract is
`include/nexix/kinfo.h`; independently specified sizes/offsets are tested by
`tests/header_abi.c`. A 48-byte header precedes tightly spaced records:

| Header field | Meaning |
|---|---|
| version, kind | Requested version and table |
| record_size, count | Record stride and dense record count |
| header_size, flags | 48; NX_KINFO_RESIDENT (resident memory, no disk swapping) |
| total_bytes | Header plus count times record size |
| ticks, tick_hz | PIT uptime at capture; 100 ticks/second |
| reserved | Zero |

| Kind | Record bytes | Information |
|---|---:|---|
| PROCESS | 144 | PID/parent/group; real/effective IDs; V7 state/flags/priority/nice/CPU age; pending signals; zombie wait status; controlling device; waiting boolean; memory sizes; own/child CPU ticks; start epoch; remaining alarm ticks; text ID; 14-character command name in a zero-terminated 16-byte field |
| FILE | 40 | File ID, shared reference count, V7 flags, native open flags, inode ID, shared byte offset, device/inode identity and regular/pipe/TTY/storage kind |
| INODE | 40 | Inode ID, references, device/inode identity, mode/owner/flags, links, byte size and special-device number |
| TEXT | 32 | Text ID, references/resident references, V7 flags, byte size and executable inode identity; includes retained unused sticky text |
| MOUNT | 48 | Mount ID, root/read-only/failed flags, device and mountpoint identity, filesystem/device block counts, inode capacity, buffer-cache capacity/occupancy |
| MEMORY | 32 | Managed page total, free/allocated pages, page size and resident-memory flag |
| LOAD | 32 | 1/5/15-minute fixed-point scheduler averages, scale and last sampled runnable/process counts; see [uptime](uptime.md) |
| IO | 96 | CPU user/nice/system/idle ticks; successful block read/write sectors and device-transfer errors; cache lookup/hit counters; TTY input/output bytes |

IDs are table slots plus one, with zero meaning absent. They are identities for
joining a capture, not kernel addresses, durable handles or mutation targets; slots
can be reused after release. Records are ordered by slot and omit unused entries.
Unpublished SIDL processes (including bootstrap slot zero) are omitted; stopped
and zombie processes remain visible until reaped. Zombie CPU fields retain their
own/child breakdown. Waiting is only a boolean, never a wait-channel address.
Command is the V7 u_comm name, not an argument-vector or stack dump. Timestamps
are UTC epoch values; timezone formatting belongs to userland.

Page totals cover allocator-managed pages after boot/kernel reservations and the
physical mapping limit, not all installed firmware RAM. Allocated = total - free;
there is no fabricated swap statistic. File refs count shared open-file references,
not separate opens of the same inode. Pipe backing pointers and device-private
handles are omitted. Inode metadata follows successful filesystem inode writes,
including size, link count, truncation, mode and owner changes, on every mounted
volume. Failed metadata writes do not publish new resident values. Failed volumes
are marked in MOUNT. Inode timestamps remain available through stat/fstat rather
than disk reads during snapshot capture.

I/O counters are global, cumulative since boot (including bootstrap/tests), wrap
modulo 2^64, and are never reset by reading. Block counters count completed 512-byte
sectors across RAM and IDE devices; device callback failures count as errors,
while argument/bounds/read-only rejection does not. Partial transfers count their
completed sectors before an error. Cache lookups/hits count V7 filesystem cached
block reads. CPU user ticks at nice > 20 are separated into nice_ticks; system
means a kernel-mode tick with a current process and idle means no current process.
Their sum equals uptime ticks. IRQ-masked synchronous I/O cannot provide meaningful
historical disk-busy fractions/seek estimates; these records do not invent them.
TTY input counts bytes delivered to tty_receive (including control/dropped bytes);
output counts physical line-discipline bytes after CR/tab expansion and includes
echo. Kernel boot/debug messages bypassing the TTY are excluded.

Reserved fields and all unused command bytes are zero. Published fields are
explicit values, not copies of private structs. New incompatible layouts require
a new version. Later diagnostic consumers must check version, stride and lengths.

## Verification

`make test-kinfo` runs host publication/bounds/permission fixtures, metadata/cache/
block-counter fixtures, QEMU filesystem acceptance at 16 and 128 MiB, and header,
export, reference-inventory and roadmap checks. Native tests cover real file/dup
references, inode joins and metadata updates, nested/read-only mounts, memory,
cumulative I/O, own-process fields, zombie publication/reaping, rejected read-only
user buffers and all seven unprivileged query/data requests. `make test-unit`
includes these host fixtures plus TTY byte-counter checks. `make test-shell` checks
RAM and IDE shells at both RAM sizes. The V7 command ports and raw memory devices
remain their separate roadmap tasks.

Multiplexor descriptors (task 65) use file kind `NX_KINFO_FILE_MPX` (5).
Socket descriptors (task 425) use `NX_KINFO_FILE_SOCKET` (10); their native
discriminator bit is not reported in the V7 flags.
Their flags retain V7 FMPX/FMPY side bits; a group has both bits set. File
reference counts include group joins and shared extractions. Live group inode
records expose IFMPC even though backing storage retains IFCHR; after group
closure, any retained endpoint inode reports the inactive character-device mode.
See [multiplexor ownership](multiplexor.md).

Task 69 adds `NX_KINFO_FILE_MEMORY` (6) for memory/null character devices,
without changing snapshot version or record layouts. See [memory devices](memory-devices.md).

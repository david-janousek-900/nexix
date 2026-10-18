# V7 disk ordering and drive interleave (task 73)

`kernel/v7/disk_order.c` adapts `usr/sys/dev/dsort.c` and `dkleave.c`.
`kernel/v7/disk.c` integrates them into the native HP strategy path;
`kernel/v7/storage.c` protects every physical member of an interleaved view.
The other disk families retain their existing synchronous transfer paths.

## Request ordering

Each HP binding owns a request queue. Requests carry separate cylinder,
completion/error, link and read/write fields rather than reusing V7's
`b_resid` buffer member as `b_cylin`. Cylinder numbers use the mapped physical
block plus partition offset, divided by the source's 418 blocks per cylinder.

Insertion follows the supplied `disksort` algorithm exactly: it scans existing
requests, considers reads and writes separately, chooses the largest eligible
cylinder predecessor, and wraps to the tail when no predecessor exists.
The existing head stays first, including when it is active; equal-cylinder
requests retain the source's insertion order. A read encountering a write
ends the scan. This is the historical algorithm, not a replacement ascending
sort or an unconditional read-priority scheduler.

The HP block callbacks submit and drain a request through this queue. Nexix's
current PIO/file path is synchronous and serialized with interrupts disabled,
so ordinary callbacks normally have only one queued request. No asynchronous
I/O syscall, concurrency or throughput improvement is claimed. Internal
callers can submit a batch to exercise and use the same real dispatch path.
The kernel QEMU fixture submits three real ATA reads before dispatch.

Internal batch callers zero-initialize requests and retain their buffers and
views until completion. Dispatch preserves the active head during backend
callbacks, completes failed requests and continues the queue. Each request
records its own error. Existing HP retries remain 28 retries (29 attempts),
with failed reads staged privately. Re-entering a synchronous callback on its
currently draining binding returns EBUSY before queuing a stack request.
Queued requests prevent `disk_release` from dropping their view/member pins:
it returns EBUSY until all requests complete. This interface is kernel-only.

## HP interleaved minors

The low-byte minor retains the V7 octal `idp` encoding: interleave flag
`0100`, highest included drive `d`, and partition `p`. Without the flag,
`dkblock` leaves the block unchanged and `dkunit` selects `minor >> 3`.
With the flag:

```
drives         = ((minor >> 3) & 7) + 1
physical unit  = logical block % drives
partition block = logical block / drives
physical block = partition first + partition block
```

The helpers implement the full source encoding; the HP driver validates the
snapshot's `NHP=2` configuration. Supported block views therefore include
one-drive and two-drive interleaving. For example,
`NX_HP_INTERLEAVED_MINOR(2, 1)` is octal `0111`: partition 1 striped across
HP units 0 and 1. Use `NX_DISK_DEV(NX_DISK_HP, minor, 0)` with `mknod`.
Existing generated HP nodes retain their original noninterleaved numbers.
No existing persistent root is rewritten.

Capacity is the source per-drive partition length multiplied by the number
of drives. All members must exist, have sufficient extent and have distinct
native backing identities. High/unsupported minor bits, more than two drives,
unassigned partitions and duplicate backing members return ENXIO; absent
members return ENODEV. Validation finishes before any member reference is
acquired, so failed opens cannot leave partial pins. Each selected member
stays pinned until final view release.

Block reads/writes cross stripes at 512-byte boundaries. Partial writes merge
only the selected physical sector. The quotient/remainder mapping preserves
the historical byte layout, including first/last sectors and nonzero partition
offsets. A read at the aggregate end returns disk EOF; writing there fails EIO.

The V7 hp(4) manual explicitly says raw drivers do not work on interleaved
devices. Nexix rejects interleaved raw opens with ENXIO, including one-drive
flagged raw minors, rather than silently performing an incorrect contiguous
DMA-style transfer. Noninterleaved raw access retains task 53/70 behavior.
There is no new syscall or libc function.

## Aliases, protection and persistence

A striped view owns one physical extent on each member. Mounted-overlap checks
compare every member against all physical extents of another view. A mount on
either drive blocks overlapping striped writes/mounts; a striped mount blocks
overlapping noninterleaved, raw, cross-family and legacy whole-disk writes.
Already-open descriptions obey the same live checks. Adjacent, disjoint
partitions remain writable. Filesystem writes through their mounted device
remain permitted. If any member becomes read-only, the entire aggregate is
read-only for writes; readonly state is checked again at transfer time.

Binding replacement/removal while pinned remains EBUSY. Missing/small-member
or readonly-open failures release all resources. Interleaving supplies no
redundancy, cross-sector atomicity or journal: a failed multi-sector write can
leave its completed prefix, just as other native block-device writes do.

## Validation

`make test-disk-order` includes:

* A differential harness that compiles **unchanged source bytes** from the V7
  reference using minimal host type shims, then compares 131072 queue
  insertions and 262144 block/unit mappings against the native helpers.
* Host tests for mixed read/write dispatch, cylinder wrap/ties, queue reuse,
  completion/error continuation, retry counts, recursive-callback rejection,
  pending ownership, member validation, last blocks, live write protection,
  both directions of mounted-alias protection and adjacent partitions.
* Full guest syscall acceptance at 16/128 MiB, plus two disposable ATA members
  hosting a striped V7 filesystem. Real queued reads and persistent 700-byte
  file writes exercise both physical drives across two cold boots. Python
  independently deinterleaves the disk bytes and reads the V7 files.
* A further boot with an unmarked member verifies that neither auxiliary disk
  is changed. The test moves its CD-ROM to secondary slave so primary slave
  and secondary master can supply the two HP members. The working
  `build/nexix.img` is never attached or modified.

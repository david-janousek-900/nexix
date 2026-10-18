# DECtape block devices (task 72)

The native equivalent of V7 `usr/sys/dev/tc.c` exposes eight TC-11/TU56
units as `/dev/tap0` through `/dev/tap7`. Each is a block device containing
578 independently addressable 512-byte blocks (295936 bytes). Reads and
writes can seek in either direction and overwrite individual blocks without
truncating subsequent data. There are no sequential records, tape marks,
rewind-on-close options, raw character devices, or TC device ioctls.

## Native interface

Generated roots install block nodes major 22, minors 0..7, mode 0600.
`<nexix/dectape.h>` supplies `NX_TC_DEV(unit)`, `NX_TC_BLOCKS`, `NX_TC_BYTES`
and media family `NX_TAPE_TC`. Use `NX_TC_DEV`, not the HT/TM device-number
macros. Existing working disks are preserved; create these nodes when
upgrading an older root. Opening an unloaded native unit returns ENODEV;
this replaces the original null open routine and physical-media probing at I/O.

Media loading shares task 71's native `/dev/tapectl` interface. This is a
native extension; the original `tc` switch has a null open routine, a close
routine and a block strategy, with no character switch entry.

* `NX_TAPE_LOAD` takes `struct nx_tape_load`, family `NX_TAPE_TC`, unit 0..7,
  an existing regular-file path, flags, and zero capacity/reserved fields.
* Without initialization, the file must be exactly 295936 bytes. Sparse files
  of that logical size are accepted. Their holes read as zero; later writes
  can fail if the containing filesystem runs out of space.
* `NX_TAPE_INITIALIZE` requires an empty writable file and explicitly writes
  all 578 zero-filled blocks. It never silently reformats existing contents.
  A failed initialization leaves the partially written file available for
  inspection/removal and releases the inode pin; no unit becomes loaded.
* `NX_TAPE_READONLY` omits the backing write callback. It cannot be combined
  with initialization. Writable opens of this media fail with EROFS.
* `NX_TAPE_UNLOAD` requires only family/unit; all other fields must be zero.
  It returns EBUSY while any unit descriptions or mounts remain.

Loading/unloading requires effective root on each control call. Loaded images
stay pinned until explicit unload, including after closing tapectl or process
exit. The task 71 inode lease also protects TC images: ordinary writes,
truncation, accounting/core writes and duplicate HT/TM/TC attachments are
rejected. Existing ordinary writers prevent loading. A loaded image may be
unlinked; its blocks remain until final unload. A mount containing a loaded
image is busy until that image is unloaded.

Units permit multiple opens, unlike HT/TM transports. Independent opens have
independent byte offsets; dup/fork share their original description's offset.
Partial writes use the common block-device read/modify/write path. Requests
starting at block 578 or beyond fail with EIO, including reads: V7 tcstrategy
marks out-of-range blocks as errors, rather than treating them as disk EOF.
Zero-count I/O transfers nothing. Partial transfers up to the boundary are
returned before a following access reports EIO. Native minor values above
seven are rejected instead of feeding unsupported bits to TC-11 registers.

Storage mounts pin the device, permit read-only access and reject ordinary
writes through mounted aliases, including descriptions opened before mount.
Lifetime is counted by unit across aliases, rather than by an individual
device inode. File-backed images can contain a V7 filesystem; filesystem I/O uses the same
TC block callbacks. I/O completes synchronously through the existing
write-through filesystem/device paths, so there are no delayed TC buffers
left to flush at final close. Close clears the unit's persistent select error
only after its final description/mount has released the device.

## Recovery and source mapping

`tc.c`'s strategy/start/interrupt state machine becomes serialized, synchronous
block transfers. Its FIFO admission and forward/reverse search serve physical
TC-11 hardware; the native backend directly addresses the requested block.
Motor motion, register/DMA emulation and interrupt timing are not reproduced.
The source's observable unit selection, block bounds, completion, retry and
close/error contracts are preserved.

EIO transfers receive at most 20 attempts, matching the source error counter.
A failed read never publishes a partially filled backend buffer. Exhausting
ordinary retries fails that request without latching `tcper`. Native ENODEV
or ENXIO backend failures represent illegal-operation/select failures: the
first call returns that error and subsequent requests on the unit return EIO
until final close. Other units remain usable. EROFS and other non-EIO failures
are terminal without retries. Unlike real hardware, failures after partially
writing a block cannot be rolled back; the containing V7 filesystem and ATA
controller determine durability. No power-loss journal is added.

Device snapshots classify TC descriptions as block storage; no new syscall,
libc function or snapshot-record layout is introduced.

## Format fixtures and tests

TC images are headerless block bytes, not task 71's NXTAPE1 format. The driver
treats those bytes as opaque: either an archive, a filesystem, or another
block format can be stored on the medium.

The archive fixture follows `usr/src/cmd/tp/tp.h` and `tp0.c`–`tp3.c`:
block zero is reserved for bootstrap bytes, followed by 192 directory entries
of 64 bytes (24 blocks), with data beginning at block 25. Each directory entry
has a 32-byte name, historical mode/owner/size/time/address fields, padding and
a 16-bit checksum whose 32 words sum to zero. Fixtures explicitly encode
PDP-11 word/long ordering rather than copying a native LP64 C structure.
The bootstrap fixture is zero-filled; a runnable standalone bootstrap remains
its separate roadmap task. Porting the `tp` program remains task 236.

`make test-dectape` runs host recovery/ownership tests and full guest syscall
acceptance at 16/128 MiB. Disposable auxiliary ATA images provide a tp-format
medium, a nested writable V7 filesystem, and a fully zero-initialized medium.
Two cold boots preserve directory checksums, first/last-block bytes, nested
filesystem writes and initialized data. An independent decoder checks the
actual filesystem and tape bytes, including indirect blocks. An unmarked
auxiliary image must remain byte-for-byte unchanged. The working
`build/nexix.img` is never used by these tests.

# Sequential tape devices (task 71)

The native equivalents of V7 `usr/sys/dev/ht.c` and `tm.c` use regular files
on a mounted V7 filesystem as removable media. They retain exclusive opens,
variable raw records, 512-byte buffered records, tape marks, spacing,
rewind-on-close and source retry limits. No PDP-11 registers are emulated.
DECtape remains task 72; archive commands have their own roadmap tasks.

## Devices and ownership

| Node | Type and device | Contract |
|---|---|---|
| `/dev/tapectl` | character 21:0 | Load/unload regular-file media; effective root required |
| `/dev/ht0`, `/dev/rht0` | block 17:0, character 19:0 | One HT unit |
| `/dev/tm0`, `/dev/rtm0` | block 18:0, character 20:0 | TM unit zero; minors 0..7 select eight units |

Generated roots install these nodes with mode 0600. Existing persistent roots
are preserved; create nodes with `mknod` from a native program when upgrading.
The root image now reserves 96 inodes to leave space for the additional nodes
and temporary mount fixtures. Other fixture volumes retain their existing size.
The private set-ID acceptance probe uses the existing verified ELF embedding
helper to omit non-runtime symbols, leaving room for its temporary filesystem
copies; installed executables and debugger/symbol fixtures retain symbols.

Minor bit 0200 disables rewind on close. HT bit 0100 selects 800 bpi;
otherwise HT reports 1600. This follows **ht.c**, whose density selection
contradicts the prose in `ht.4`. TM reports 800. Density is informational for
native images. HT unit bits are minor & 077, TM minor & 0177; unsupported units
fail with ENXIO rather than indexing outside the source driver's arrays.
HT retries EIO ten times total; TM twice, matching their source counters.

Loading pins the canonical regular-file inode until explicit unload, even after
closing tapectl or exiting the process that loaded it. The image cannot be attached
twice, opened for ordinary writing, truncated, or used for accounting/core
writes while loaded. Loading also rejects an already-open writer. Its mount
cannot be unmounted until unload. Unlinking a loaded image is allowed: the
inode and its blocks survive until unload. Readers may inspect the bytes.
Changing the underlying disk through another operating system is unsupported.

A unit has one open description across all raw/buffered/density/no-rewind
aliases. Dup/fork share that description and physical position. Only the last
close writes trailing marks and performs automatic rewind. Open resets the
error state and validates the image header, without physically rewinding.
Non-data transport/format failures poison the open unit until close/reopen;
a caller buffer shorter than its record consumes that record and returns EIO
without poisoning the transport. This explicit size-error rule also applies
to HT, whose hardware frame-count handling differs in the original source.

Write-only close writes two tape marks even without a data write. Read/write
close does so only after a successful data write. It then backs up one mark,
leaving no-rewind media between the marks. Ordinary close rewinds to BOT.
Close errors are returned while still releasing the open description.

## I/O and native controls

Include `<nexix/tape.h>` and use libc's existing `open`, `read`, `write`,
`lseek`, `ioctl` and `close`; no new syscall numbers or libc entry points.
Raw reads/writes transfer one record. Nonzero raw buffers and counts must be
even (EFAULT otherwise); writes are bounded to 65534 bytes. A mark returns
zero bytes and advances to the next file; published end-of-data also returns
zero. Zero-count I/O does not move the head or write a mark. Raw lseek changes
the descriptor offset but has no effect on the physical head, as in V7.

Buffered I/O uses 512-byte records and byte offsets relative to the position
at open or the latest explicit control operation. Partial writes merge an
existing record or zero-fill a new one. Seeking traverses records without
crossing a tape mark; sparse tape writes beyond end-of-data fail. A non-512
raw record read through the buffered interface returns EIO. Writing at any
position discards the following published suffix, as with physical tape.

V7 configures **nodev for ht/tm ioctl**. The following controls are a documented
native extension, not a claimed historical MTIO ABI:

* `NX_TAPE_LOAD` on tapectl takes `struct nx_tape_load`: family HT/TM, unit,
  flags, path pointer, capacity. The file must already exist and be regular.
  `NX_TAPE_INITIALIZE` requires an empty writable file and capacity 96 bytes
  through 64 MiB; existing contents are never silently formatted.
  Loading an existing image requires capacity zero. `NX_TAPE_READONLY` omits
  the backing write callback and cannot be combined with initialization.
* `NX_TAPE_UNLOAD` on tapectl uses family/unit with other fields zero; open
  units return EBUSY. Reserved fields and unknown flags are rejected.
* `NX_TAPE_STATUS` on a unit returns the 64-byte native status record:
  family/unit, online/open/writable/error flags, density, record index, byte
  position, published record count/end, capacity and residual count.
* `NX_TAPE_OPERATION` on a unit takes operation/count. Operations rewind,
  space forward/backward records or files, write marks, or erase the suffix.
  Counts are 1..65536; rewind/erase require one. File spacing also has a
  65536-record work limit. Forward-file stops after a mark; backward-file
  stops before a mark. BOT/EOD spacing returns ENXIO with partial movement
  retained. Writing marks/erasing requires a writable description (EBADF).
  An attempted valid movement resets the buffered origin and descriptor offset,
  including partially completed spacing. Invalid requests leave them unchanged.

Device snapshots identify these descriptions as `NX_KINFO_FILE_TAPE` (7)
without exposing the file layer's private discriminator bits.

## Persistent format and failure bounds

Images are explicitly native, versioned little-endian files, not SIMH tape
files or PDP-11 controller dumps. The 64-byte header contains:

| Offset | Field |
|---|---|
| 0 | Eight bytes `NXTAPE1\0` |
| 8, 12 | uint32 version 1, header size 64 |
| 16, 24 | uint64 published end, record count |
| 32, 40 | uint64 capacity, generation |
| 48 | uint32 FNV-1a checksum over bytes 0..47 and 52..63 |
| 52..63 | Reserved zero bytes |

Every record has an eight-byte header (uint32 length, uint32 FNV-1a payload
checksum), payload padded to an even length, and an identical eight-byte
footer. A mark has zero length/checksum and occupies 16 bytes. Readers accept
odd records in imported native images, although raw guest writes require
even counts. Record count is bounded to one million. Payload checksums are
verified before copying record data to user memory; frame/header corruption
returns EIO. Records are validated lazily during traversal, avoiding a large
kernel index or an unbounded scan at open. Trailing unpublished file bytes
are ignored; physical inode size can exceed the published tape end.

Append stages a record before publishing the checked header. Overwrite first
publishes the preceding valid prefix, then replaces the suffix. Failed
appends leave the old published prefix; failed overwrites may lose the old
suffix. There is no journal or power-loss atomicity guarantee. Torn header
writes fail checksum validation on reload. The underlying V7 filesystem and
ATA controller still determine durability and storage failure behavior.

## Validation

`make test-tape` runs host framing/recovery tests, real syscall acceptance,
root ownership cleanup, ABI/inventory audits, and two cold boots on disposable
auxiliary ATA media at both 16 and 128 MiB. An independent Python decoder
checks the persisted header, payload and two marks. An unmarked auxiliary
image is hashed before/after to verify that the private fixture does not
write to unrelated media. The working `build/nexix.img` is never used.

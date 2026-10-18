# Native V7 disk-controller equivalents (task 70)

`usr/sys/dev/hp.c`, `rf.c`, `rk.c`, `rl.c` and `rp.c` supply the original
partition tables, unit selection, logical capacities, raw entry points and
recovery responsibilities. Supporting sources are `dev/bio.c`, `h/buf.h`,
`h/conf.h`, `conf/c.c`, `conf/mkconf.c`, `sys/fio.c` and `sys/rdwri.c`.
The snapshot supplies hp/rf/rk/rp(4) manuals, but no rl(4). References remain
unchanged. Controller register operations, UNIBUS DMA, cylinder/head seeks and
interrupt completions are replaced by synchronous native LBA media operations.

`kernel/v7/disk.c` supplies five families of bounded native block-device views.
`kernel/v7/storage.c` connects these to open/read/write/seek/mount/umount and
preserves task 53's buffered/raw user-copy contracts. `kernel/ide.c` supplies
four ATA device selectors, bounded PIO waits and channel reset after transfer
errors. `kernel/disk_native.c` discovers auxiliary ATA disks for normal boots.
No PDP-11 MMIO addresses are accessed by Nexix.

## Devices and backing

All device numbers use the existing high-byte major/low-byte minor encoding.
Fresh roots include a mode-0600 block and raw node for unit/partition zero:

| Family | Block major/node | Raw major/node | Minor meaning |
|---|---|---|---|
| HP | 7, `/dev/hp0` | 12, `/dev/rhp0` | `unit*8 + partition`, two units |
| RF | 8, `/dev/rf0` | 13, `/dev/rrf0` | Aggregate prefix of `1024*(minor+1)` blocks, minor 0–7 |
| RK | 9, `/dev/rk0` | 14, `/dev/rrk0` | Unit 0–3; 4872 blocks each |
| RL | 10, `/dev/rl0` | 15, `/dev/rrl0` | Unit 0–3; 10240 logical 512-byte blocks each |
| RP | 11, `/dev/rp0` | 16, `/dev/rrp0` | `unit*8 + partition`, eight units |

Constants and device encoding helpers are in `<nexix/disk.h>`. Existing root
block major 0, raw major 4 and other device numbers remain unchanged. Other
units/partitions can be created with mknod(2); the standalone command still
belongs to its later command-port task. Existing persistent roots are preserved
and do not automatically gain the new nodes. Select a fresh working disk with
`make DISK_IMAGE=build/nexix-task70.img run` when testing packaged nodes.

The primary-master ATA disk remains the ordinary root backing. Auxiliary
primary slave, secondary master and secondary slave map to family units 0, 1
and 2 respectively, subject to each family's unit limit. RF has a single
aggregate backing at unit 0. Unsupported/absent media are skipped; absent
bindings fail ENODEV. ATA uses validated 512-byte sectors, LBA28 and cache flush
on successful writes. A raw disk can be attached to QEMU with, for example,
`-drive file=auxiliary.img,format=raw,if=ide,index=1`.

Every applicable family views the same auxiliary disk; these are alternate
interfaces, not five independent copies of its bytes. Kernel configuration may
instead use `disk_bind(kind, unit, backing)` with a stable native block device.
RF's backing represents the combined flat RF file, rather than requiring native
hardware to reproduce individual RS-11 units. Nested controller views are
rejected. Disk binding itself allocates no memory and changes no disk bytes.
The binding owner retains backing lifetime until all bindings are removed.

## Partition and geometry contracts

HP offsets and lengths, in logical 512-byte blocks:

| Partition | First | Length |
|---|---:|---:|
| 0 | 0 | 9614 |
| 1 | 9614 | 8778 |
| 2–3 | — | Unassigned |
| 4 | 18392 | 161348 |
| 5 | 179740 | 160930 |
| 6 | 18392 | 153406 |
| 7 | 18392 | 322278 |

These are the supplied cylinder offsets multiplied by 418. A view must fit
completely in its native backing: RP04-sized media cannot open RP06-only
extents. No view silently truncates or wraps an address.

RP retains partitions 0=(0,81000), 1=(0,5000), 2=(5000,2000), and
3=(7000,74000); partitions 4–7 are unassigned. RF prefixes overlap from block
zero. RK and RL expose their fixed logical capacities even on larger native
media. RL's historical 256-byte hardware sector/track splitting is replaced by
native 512-byte sector transfers, which work across logical track boundaries.

Invalid units/minors, unassigned partitions and extents larger than the backing
fail ENXIO. The final declared sector is accessible. This corrects rp.c's
`block + size >= nblocks` off-by-one rejection and follows its declared partition
sizes/manual. Unchecked RK/RL unit selectors and RF address wrapping are also
rejected. RL exposes the four entries supported by its drive-state array;
the supplied `RL_CNT=1` define is unused in that driver's strategy routine.

HP interleaved minors are not accepted by this noninterleaved view interface.
Their distribution and disksort/dkleave queue behavior remain task 73. Physical
head offsets, software ECC bit corrections and seek calibration are not applied
to unrelated ATA media: the backing controller handles physical media/ECC, and
unrecoverable faults propagate through native error results.

## I/O, recovery and ownership

Sector read failures are staged in a private buffer so failed attempts do not
publish damaged bytes. EIO gets up to 28 HP retries or 10 RF/RK/RL/RP retries,
matching the supplied drivers' limits. Other errors return immediately.
Retry state resets for each sector; subsequent requests can recover after an
exhausted request. ATA errors reset the affected channel before resubmission;
all polling waits, including reset completion, are bounded. Cache flush errors
are failures. A failed write may already have changed backing bytes, as with
ordinary device errors; there is no transactional rollback of physical writes.

Partial multi-sector requests preserve the completed byte count through the
existing storage/file layer. Raw I/O retains even-address/even-count checks and
rounds the starting offset down to its sector, as task 53 specifies. Partial
writes use read/modify/write and preserve untouched tail bytes. Reads exactly at
view end return EOF; writes there fail EIO. O_APPEND/O_TRUNC do not resize or
reposition device extents; stat preserves zero inode size and native rdev.

There are 32 active controller-view slots. Repeated opens of the same device
share a driver handle; block and raw aliases retain independent view identities
with the same backing extent. Binding replacement/removal while referenced is
EBUSY, including mounted volumes. Final close releases the binding reference
and makes the slot reusable. A legacy slot cannot drop a backing still owned
by a controller binding. Slot exhaustion is ENFILE.

Mounted-volume protection compares backing identity and overlapping extents
across all five families, legacy whole-disk slots, block/raw aliases and
already-open handles. Overlapping writes/mounts fail EBUSY; adjacent partitions
can remain independently writable or mounted. Filesystem writes through the
mounted view itself remain permitted. This extends task 53's cache-coherency
guard without pretending overlapping aliases are independent volumes.

## Validation

Run `make test-disk`. Host tests cover every declared HP/RP partition and unit,
RF prefix, RK/RL unit, first/last sectors, bounds, offline/readonly/small media,
transient/exhausted errors, retry limits, mounted aliases, adjacent mounts,
handle exhaustion/reuse and binding lifetime. ATA register models exercise
primary/secondary master/slave isolation, flush errors, missing/unsupported
devices, timeouts and reset recovery. Root-image/header audits check node modes
and device encodings.

Ordinary acceptance boots exercise all five family nodes through real syscalls
with an isolated sparse fixture. A separate QEMU peer test attaches a disposable
174 MB sparse ATA disk, mounts a V7 volume through RP partition 2, writes data,
checks overlapping raw/whole-disk protection and adjacent-partition access,
unmounts/remounts and repeats on a second cold boot at 16 and 128 MiB. Python
independently checks persisted V7 file bytes and the adjacent partition marker.
The writable auxiliary test requires its fixture signature in sector zero;
a further boot verifies that unmarked auxiliary media are skipped before mount
or write and retain their entire disk hash. Working
`build/nexix.img` is never used by these tests.

The private acceptance fixture stores its held frame addresses in 32-bit entries,
with a compile-time assertion covering the allocator's 1 GiB address limit.
This saves 128 KiB of kernel BSS while retaining the original fixture capacity
and the 16 MiB boot acceptance requirement.

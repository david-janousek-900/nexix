# Multiplexor ownership and data/control (tasks 65–67)

Nexix ports group/channel ownership and record data/control I/O from
V7 `usr/sys/dev/mx1.c` and `mx2.c`, with limits from `usr/sys/h/mx.h` and the
interface described in `usr/man/man2/mpx.2`. Reference files remain unchanged.
The private implementation is `kernel/v7/mpx.inc` and `mpx_io.inc`, included by
`files.c` to use its serialized descriptor, open-file and resident-inode tables
directly. Queue operations use the same IRQ serialization as pipes.

The native raw interface is syscall 56, `(command, &arguments)`. Include
`<nexix/mpx.h>` for command numbers and `struct nx_mx_args`: an eight-byte name
pointer, a four-byte historical command field (unused), and three signed
four-byte arguments. High-level libc wrappers are declared in `<sys/mx.h>` (task 67).

| Command | Inputs | Result |
|---|---|---|
| MPX (5) | name; arg[0] permissions | New named group descriptor |
| MPXN (6) | No inputs | New anonymous group descriptor |
| CHAN (1) | arg[1] group descriptor | New channel index relative to the tree root |
| JOIN (2) | arg[0] child group; arg[1] parent group | Local parent branch number, 0–14 |
| EXTR (3) | arg[0] index; arg[1] group; arg[2] endpoint side | Descriptor for a group or channel endpoint |
| ATTACH (4) | arg[0] index; arg[1] group | Accept a pending open; return the channel's root-relative index |
| DETACH (8) | arg[0] index; arg[1] group | Zero |

Errors are negative errno values in the raw ABI. Invalid group descriptors,
missing entries, and exhausted group/channel slots return ENXIO. Descriptor
and global file/inode exhaustion return EMFILE and ENFILE. Bad argument memory
returns EFAULT before mutation. Cycles and excessive nesting return EINVAL;
joining an already attached group returns ENXIO. Unsupported commands currently
return EINVAL. Named creation uses the normal path, permissions, umask, read-only
mount and duplicate-name checks. It does not require root privilege.

There are ten groups globally, twenty ordinary channels globally, fifteen
branches per group, and at most four group levels. Joined groups retain their
canonical open-file entry even after the original descriptor closes. Extracting
a group duplicates that entry. The first channel extraction selects its endpoint
side; later extractions duplicate the same entry, as in V7. Descriptor duplicates
share state; fork retains them, successful exec closes FD_CLOEXEC descriptors,
failed exec preserves them, and exit closes them normally. Join checks include
the entire subtree's height and reject cycles and multiple parents.

Indices retain V7's sixteen-bit nibble paths: the low nibble is the branch at
the root, each following nibble descends one group, and F terminates a shorter
path. Results are zero-extended to a positive native int so historical negative
short indices do not collide with native negative errno. Inputs also accept
sign-extended historical shorts. For example FFF0 selects branch zero of the
supplied group, FF10 selects branch zero then branch one, and 1000 traverses
four branches. The manual's bare 0–14 abbreviation is supported for a direct
channel. Use a terminated index such as FFF0 to extract or detach a child group;
a full four-nibble path such as 0000 can identify a deeply nested channel.
JOIN returns a local branch; add the terminating F nibbles when using it as a
group index. CHAN returns a path through any joined ancestors.

Closing an endpoint while attached leaves a closing channel owned by its group
until detach or group destruction. Detaching a live endpoint marks it closing
and keeps its branch reserved until the final endpoint descriptor closes.
Destroying the parent releases its branches immediately and leaves live
endpoints independently closable, without dangling group pointers. Parent
closure recursively releases joins. No queue storage or TTY pointer is aliased
with these ownership objects.

Groups use real V7 character-device inodes with native device major 5 and the
group slot as minor. Anonymous inodes have nlink zero and are reclaimed at the
last inode reference. Named inodes remain linked after group closure and become
inactive character devices, as in V7. While a group is live, stat/fstat and
kernel-information inode records expose IFMPC (0030000). This is a resident
view: the backing inode remains IFCHR, so reboot cannot revive a stale kernel
multiplexor. Lookup uses the full filesystem device/inode identity. Open files
and endpoint inode references prevent premature reclamation and unmount.
Kernel-information file records identify these descriptors as FILE_MPX (5).

## Data and control records (task 66)

Include `<nexix/mpx.h>` for native layouts. A group read returns six-byte
`nx_mx_read` headers (`uint16_t index, count, ccount`) followed immediately by
payload. Data uses count; controls use ccount with count zero. Records start
on two-byte boundaries; padding bytes are zero. The final odd payload needs
no padding if it exactly exhausts the supplied buffer. Group read buffers
must have even addresses (ENXIO otherwise). A buffer shorter than twelve bytes
returns zero. Control records are indivisible: an insufficient buffer returns
already completed records, or zero, and retains the pending event. Otherwise,
an empty group blocks interruptibly. Ready branches rotate at each tree level.
As with task 65 CHAN, read indices include joined ancestors to the tree root.

Group writes accept arrays of sixteen-byte `nx_mx_write` records: uint16 index,
uint16 count, int16 ccount, a zero uint16 reserved field, and an eight-byte native
payload pointer. There is no extra record-count limit. A data record has count
1–32767 and ccount zero. A control record has count zero and ccount four, with a
`nx_mx_control` payload containing uint16 code/value. Group writes currently
accept M_EOT and native M_IOCTL controls. Task 67 completes the reference's
mxwcontrol M_IOCTL branch, which only prints a diagnostic in V7.

Headers must be writable, including for blocking calls, so an interrupted
partial record can report exactly which bytes remain. The complete header
array and payload ranges are checked before queue changes; each descriptor
is copied and validated again before use because shared physical aliases can
change later records while a writer sleeps. Invalid memory returns EFAULT;
malformed lengths, reserved fields, counts and truncated header arrays return
EINVAL; missing channels and unsupported control codes return ENXIO. These
checks replace unsafe historical copy/length assumptions. Earlier completed
records remain transferred if a later record changes or disappears while asleep.

An extracted Y endpoint reads ordinary bytes from the group-to-endpoint queue
and writes bytes to the endpoint-to-group queue. X endpoints retain V7's
opposite-side selection: they read the same queue as group reads and write the
same queue as group writes. The first extraction continues to determine side.
Each direction has a 200-byte ring, matching V7's HIQ=100 and 100-byte transfer
chunks: a write can grow a queue from 100 to 200 bytes, then sleeps above HIQ.
Readers wake writers below LOQ=20. In-flight group writes pin channel objects
across sleeps, so detach, close and immediate branch reuse cannot redirect an
old writer into a new channel. Endpoint writes after detach/group loss raise
SIGPIPE and return EPIPE if no bytes moved; reads drain queued bytes before
returning ENXIO. Signal interruptions return EINTR when nothing transferred.

`ioctl(group, NX_MXNBLK, 0)` enables V7 message-based nonblocking group writes.
When a queue or pending EOT prevents completion, write-back sets ccount to -1,
count to the remaining byte count, and advances data past transferred bytes.
Reset ccount to zero to retry the remaining data record. Preserve mxwrite's
source-defined return convention: bytes of descriptors processed minus the
number of truncated records. For one sixteen-byte record, full transfer returns
16 and truncation returns 15. An interrupted blocking write that already moved
part of a record uses the same feedback instead of losing its retry position;
subsequent records remain untouched. These calls never silently discard bytes.

The preserved mx2.c has its data-write M_BLK enqueue commented out; Nexix follows
that source's header feedback and delivers M_UBLK once the affected queue drains
below LOQ. A repeated group M_EOT in nonblocking mode instead reports M_BLK with
value zero. Blocking repeated EOT waits for the prior marker to be consumed.
A group EOT allows preceding data to drain, then the endpoint reads zero once;
subsequent writes resume. A zero-length endpoint write sends M_EOT to the group.

Control events are bounded separately: sixteen queued records per channel,
with up to fifty pathname bytes for M_OPEN. Zero-length endpoint writers wait
interruptibly when the event ring fills. Close and block/unblock notifications
have reserved pending state so a full event ring cannot lose them; repeated
state notices coalesce. Queued controls have priority over data, followed by
pending close/block/unblock notices. The final endpoint close posts M_CLOSE;
closing a duplicate does not. The group owner drains remaining data and uses
DETACH to reclaim the channel. Group destruction also releases all queued state.

## Named opens and listeners

Opening an active named group allocates a Y channel, sends M_WATCH with the
opener's effective UID, and waits for ATTACH. The open file is not published to
the caller until acceptance. Accepted channel descriptors retain the requested
O_RDONLY/O_WRONLY/O_RDWR access mode; this native permission check replaces
V7's unconditional full-duplex grant after the pathname check. DETACH or parent closure
rejects the open with ENXIO. A signal cancels an unaccepted open with EINTR and
reclaims its unpublished file/channel/inode references. New opens are matched
by full device/inode identity, so reusing a minor number cannot revive an old
named inode. An inactive group device returns ENXIO.

`ioctl(group, NX_MXLSTN, 0)` selects the single global listener. Other attempts
return ENXIO until that group closes. Listener opens send M_OPEN instead of
M_WATCH, with the UID followed by the actual supplied pathname, truncated to
49 bytes plus NUL. Each channel owns its name, replacing V7's global name buffer
and its boundary overflow. Control headers and payloads retain sixteen-bit
fields; the native write pointer, validation and retry rules above are explicit
x86_64 adaptations. Task 67 supplies the TTY bridges and libc interface described below.

## TTY bridging and libc (task 67)

`libc/mpx.c` ports `usr/src/libc/gen/mpx.c` and replaces PDP-11 `mpxcall.s`.
`<sys/mx.h>` declares mpxcall, mpx, chan, join, connect, disconnect, attach,
detach, extract, debug, npgrp and ckill. Each call uses its own argument record;
there is no shared static vector. Errors return -1 and set errno. `mpx("", mode)`
creates an anonymous group; a null name returns EFAULT. `mpxcall(MPXN, NULL)`
supplies an empty native argument record. Other null argument records fail.
The actual two-argument mpxcall source interface is used; the snapshot's
mpxcall(2) page describes a different older calling convention.

`join(ttyfd, group)` creates a TTY port and retains the canonical TTY open file.
The descriptor must be a live TTY opened for both reading and writing; other
device handles are rejected. Single-direction CONNECT requires only the access
permission for that direction. A forbidden direction returns EBADF before any
bridge is installed.
The thirty-entry port table is separate from the twenty ordinary channels.
COM1 and its controlling-terminal alias are currently the available TTYs;
independent hardware/terminal ports remain tasks 89–93. Port extraction duplicates
the original TTY descriptor. Terminal ioctls and writes on that descriptor still
work; direct reads return ENXIO while its input belongs to a bridge. Group reads
consume the normal cooked/raw/cbreak line discipline, including one M_EOT event
for an empty cooked EOF. Group writes use normal terminal output processing.
Input can belong to only one channel/port; competing joins/connections fail with
EBUSY. Already queued ordinary channel data is drained before newly routed TTY
input. Joining a TTY does not reopen or reset it.

`connect(ttyfd, channel_fd, side)` requires an ordinary Y endpoint. Negative
side connects terminal input to group reads; positive side connects group writes
to terminal output; zero connects both. Each direction retains its TTY file,
so closing the original descriptor cannot invalidate the bridge. Multiple channels
can share output; input remains exclusive. Connect and disconnect increment a
connection generation and wake sleepers. A writer blocked on an old connection
returns ENXIO, or the existing partial-record retry feedback if a prefix moved,
instead of silently switching destinations. Detach, group closure and final
endpoint close unbind input and release every bridge reference before reuse.

`disconnect(ttyfd, channel_fd, side)` is an explicit native addition using the
same directional convention. Although V7 defines DISCON=9, it has no switch
handler or libc wrapper; mpx(2) expressly says the inverse is not provided.
Nexix implements that inverse rather than claiming it was original V7 behavior.
Ordinary channel queues remain usable after disconnection.

TTY interrupt/quit characters produce reserved M_SIG notifications containing
SIGINT/SIGQUIT instead of directly signalling the terminal foreground group.
Repeated equal signals coalesce, and a full ordinary event ring cannot lose
the pending signal state. XOFF stops bridged output; XON and terminal mode/flow
changes wake blocked writers. Message-based nonblocking writes use the existing
header feedback and M_UBLK event when output resumes. COM1 writes synchronously
to the UART, so software XOFF is its output backpressure; ordinary channel rings
retain task 66's high/low-water limits. A physical serial stream has no zero-byte
EOF operation: a group M_EOT directed at a TTY returns ENOTTY, while ordinary
process channels retain logical EOF markers.

`npgrp(index, group, pgrp)` follows the supplied mx1.c implementation: it changes
the **calling** process's group and records that group on the selected channel.
A negative group descriptor affects only the caller; zero pgrp selects its PID.
It returns the selected group. This differs from the manual's claim that it
changes a remote endpoint process. Negative process groups are rejected.
`ckill` validates the signal and returns its number, delivering through the
recorded channel process group; a TTY port has no process target and is a no-op.
In addition to possession of the channel/group descriptor, user-requested
signals apply kill's effective-UID checks to each target. A foreign-UID-only
group returns EPERM and a missing group returns ESRCH; root may signal either
UID. This native security restriction also applies to group SIGPIPE generated
by endpoint writes. Named opens capture the opener's process group.
Broken endpoint writes use that group for SIGPIPE, falling back to the current
process for a channel with no assigned group. `debug` stores the reference's
thirty diagnostic settings (ALL sets all), without a data-transport effect;
the source itself has no readers of those settings. Out-of-range indices fail
with EINVAL instead of the reference's array-boundary overwrite. PACK is an
unused reference opcode without a handler or wrapper and remains invalid.

## Native M_IOCTL completion

The reference manual describes M_IOCTL transfers, but mxwcontrol only prints
`M_IOCTL`. Nexix defines a working native twelve-byte `nx_mx_ioctl`: uint16 code,
uint16 terminal command, and eight payload bytes. It is sent as a control record
with ccount twelve. GETP/SETP/SETN use native eight-byte sgtty settings; GETC/SETC
use six bytes; GETD/SETD use four; FLUSH has no argument. Unused payload bytes are not interpreted. Unknown commands return ENOTTY. TTY setting validation
and unsupported terminal modes remain those of the existing driver (task 74
owns further historical modes). Malformed M_IOCTL lengths return EINVAL.

A group M_IOCTL directed at a TTY applies a SET/FLUSH immediately; a GET queues
an M_IOCTL reply for the group. Endpoint ioctls on connected channels apply to
the output TTY, falling back to the input TTY, and notify the group with the resulting payload. Notification capacity
is reserved before changing terminal state. An unconnected endpoint instead
queues the request and waits interruptibly for the owner to write a matching
M_IOCTL response. A GET then copies the returned payload to its caller; SET and
FLUSH return after acknowledgement. Calls are serialized per channel. Copy and
range checks happen before queue/state changes; detach and closure wake pending
operations with ENXIO. A signal cancels a waiting call with EINTR. If its request
is already outstanding, new requests wait until the owner acknowledges that
cancelled request (the acknowledgement returns EINTR) or detaches the channel.
This avoids assigning a stale reply to a newer call without adding sequence
fields to the V7-shaped payload. Replies are ordered, one per request.

The snapshot supplies the wrapper source but no separate multiplexor application
under usr/src that consumes it. Acceptance therefore calls these original wrapper
interfaces from native fixtures and exercises their original kernel/TTY consumers;
it does not introduce or claim additional installed userland programs.

## Validation

Run `make test-mpx-tty test-unit test-tty test-shell test-storage`.
`tests/mpx.c` exercises production queue/record code, ring wrap, low-water
boundaries, control retention, payload preflight, nonblocking write-back,
interrupted partial records, large descriptor arrays and pinned detach lifetime.
The QEMU root-image fixtures require NEXIX_MPX_OK, NEXIX_MPX_DATA_OK and NEXIX_MPX_TTY_OK
at 16 and 128 MiB. They cover task 65 ownership plus framed duplex data,
streaming through full queues, EOF ordering, close events, nonblocking retries,
interrupts, event-ring exhaustion, tree routing/fairness, sleeping-writer detach,
WATCH/OPEN acceptance/rejection/cancellation, effective UIDs, aliases, listener
release, stale named inodes and leak-free file/inode cleanup. Existing shell,
filesystem and cold-boot storage checks cover surrounding paths. Reference files
and installed userland program counts remain unchanged.

Task 67 adds captured-UART host checks and native wrapper/TTY tests for joins,
canonical/raw input, EOF/signal events, directional connect/disconnect, retained
files, XOFF/XON blocking and nonblocking wakeups, generation-safe disconnection,
terminal and virtual ioctl exchanges, concurrent callers, interrupted/cancelled requests, closure,
process-group delivery and reference cleanup. The private test-only input gate
is enabled only during the root-image acceptance fixture; production boot gains
no injection API.

The expanded private acceptance image reached GRUB's loader-memory limit at
16 MiB before entering the kernel. `tools/embed_test_elf.py` removes symbol
and section-name metadata from the seven embedded acceptance executables,
while preserving their full `build/*-test.elf` artifacts. Each embedding checks
that entry point and every PT_LOAD address, size, memory size, alignment,
permission and payload byte are unchanged. Kernel symbols, installed programs,
and nlist/tracing/profiling fixtures retain their original symbols. This reduces
loaded test payload by about 44 KiB and restores the 16 MiB boot; no code/data
segment or reference source is removed.

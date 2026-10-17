# Multiplexor ownership (task 65)

Nexix ports the group/channel allocation and descriptor ownership portions of
V7 `usr/sys/dev/mx1.c` and `mx2.c`, with limits from `usr/sys/h/mx.h` and the
interface described in `usr/man/man2/mpx.2`. Reference files remain unchanged.
The private implementation is `kernel/v7/mpx.inc`, included by `files.c` to use
its serialized descriptor, open-file and resident-inode tables directly.

The native raw interface is syscall 56, `(command, &arguments)`. Include
`<nexix/mpx.h>` for command numbers and `struct nx_mx_args`: an eight-byte name
pointer, a four-byte historical command field (unused), and three signed
four-byte arguments. High-level libc wrappers belong to task 67.

| Command | Inputs | Result |
|---|---|---|
| MPX (5) | name; arg[0] permissions | New named group descriptor |
| MPXN (6) | No inputs | New anonymous group descriptor |
| CHAN (1) | arg[1] group descriptor | New channel index relative to the tree root |
| JOIN (2) | arg[0] child group; arg[1] parent group | Local parent branch number, 0–14 |
| EXTR (3) | arg[0] index; arg[1] group; arg[2] endpoint side | Descriptor for a group or channel endpoint |
| ATTACH (4) | arg[0] index; arg[1] group | Existing non-closing channel's root-relative index |
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

Task 66 still owns data/control records, queues, wakeups and the named-open
WATCH/OPEN/ATTACH handshake. Nonempty multiplexor reads/writes and opens of
named group devices currently fail with ENXIO; ATTACH only validates existing
channels and does not yet accept waiting opens. Task 67 owns TTY ports,
connect/disconnect, process-group/signal controls and the complete libc wrapper
family. No working data transport or complete mpx port is claimed by task 65.

Run `make test-mpx test-unit test-shell test-storage`. The QEMU root-image
fixture exercises named and anonymous groups, mode/timestamp metadata, bad
pointers and identifiers, descriptor and group/channel limits, nested paths,
cycle/depth checks, extraction, detach/close ordering, fork retention, failed
and successful exec, close-on-exec, unprivileged ownership and leak-free table
cleanup. It requires `NEXIX_MPX_OK` at both 16 and 128 MiB. Filesystem host tests
verify anonymous inode metadata, allocation/release counts and read-only errors.
The existing shell/storage tests cover the surrounding file and persistence
paths. Test fixtures are not installed userland programs.

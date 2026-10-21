# Native storage commands

## sum (task 226)

`/bin/sum [file ...]` retains V7's rotating 16-bit checksum, five-digit checksum
field and six-column count of rounded-up 512-byte blocks. Multiple arguments
include their filenames. With no arguments it reads standard input. A literal
`-` is a filename, as in the original source, rather than a stdin option.

The active port is copied from `reference/v7/usr/src/cmd/sum.c`, with an explicit
native main signature and checked output flushing. Byte accumulation remains
masked to 16 bits; byte counts use native long. Open failures contribute 10 to
the exit status and read failures contribute 1, with later files still processed.
Output failure returns 1. There are no device writes. Directory bytes remain
readable through the V7 kernel, as for other V7 file readers.

`make test-sum` boots disposable installed roots at 16 and 128 MiB. Independent
Python arithmetic supplies expected checksums for all byte values, repeated
overflow, empty files and 512/513-byte boundaries. Fixtures also cover stdin,
multiple filenames, failed opens with continuation, and a memory-device read
error. The working disk and historical reference remain unchanged.

## mknod (task 219)

`/etc/mknod name b/c major minor` creates a block or character node through
the native root-only syscall. Major and minor are decimal values from 0 to 255;
the native device ABI stores each in eight bits. Empty values, signs, trailing
characters and overflow are rejected before invoking the syscall. Exactly `b`
or `c` selects the node type. The initial 0666 permissions are filtered by umask.

The source adapts the V7 command with explicit declarations and bounded parsing.
Usage errors return 2; syscall failures print the V7 error message and return 1
(the historical source returned success after a failed syscall). Device numbers
can describe absent hardware; creating a node does not enable a driver.

`make test-mknod` checks installed behavior on disposable 16/128 MiB guests,
including real null-device I/O, the maximum encoded major/minor, root/ordinary
user permissions, duplicate nodes, malformed arguments, umask and persistence
across two cold boots. It never writes the working disk.

## update (task 237)

`/etc/update` retains V7's background sync daemon: fork once, let the parent
return, close standard descriptors, keep available system directories open,
sync immediately and repeat every 30 seconds using SIGALRM. A failed fork now
reports an error and returns 1. Extra operands are ignored, as in the original.
This command is installed for explicit service startup; it is not automatically
added to existing installations' startup scripts.

`make test-update` runs the installed daemon on disposable IDE roots at 16/128
MiB. After its initial sync, the test sends no guest commands during a real
33-second idle interval. It independently decodes the on-disk PDP-11-word-order
superblock time and requires at least 29 seconds of advancement, then checks
that the daemon and shell remain alive. This verifies actual periodic kernel
sync and alarm delivery, rather than only the existence of a process.

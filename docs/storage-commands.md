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

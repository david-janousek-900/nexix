# Native typesetter output (task 76)

`/dev/cat` is native character device 30:0, root-owned mode 0600. It replaces
`usr/sys/dev/cat.c`'s C/A/T output interface. Open it write-only, create and close
an empty regular capture file, and pass that file's path to
`ioctl(fd, NX_OUTPUT_ATTACH, path)` from `<nexix/output.h>`. Later writes append
the exact binary C/A/T stream to the pinned capture. Existing/nonempty captures
are refused; this interface never truncates a file implicitly. troff's consuming
port remains a later task.

A second open returns ENXIO, as in the reference. dup/fork share the existing
open description; final close releases both exclusivity and the capture inode.
Ordinary writers, executable-text users and other image backends cannot modify
an attached capture. A capture must be a writable regular file and attachments
obey current filesystem credentials. There is no privilege override. Missing
capture returns ENXIO on write, bad pointers EFAULT, reads EBADF, seek ESPIPE,
and unknown ioctls ENOTTY. Empty writes succeed without a capture.

The native synchronous filesystem transport replaces the DR11C ready interrupt
and 100/30-byte queue watermarks. It provides immediate backpressure through
synchronous writes instead of unbounded buffered output. Output is neither
rendered nor converted. Storage failures return the error or the number of
previously completed 512-byte chunks. A failed capture rejects further writes
with EIO until close, avoiding ambiguous replay after a partial backend failure.
An underlying failing chunk can have a partial filesystem write; inspect/discard
the capture after a failure. No real C/A/T hardware or PDP-11 MMIO is emulated.

`make test-typesetter` checks all 256 byte values across chunk boundaries,
exclusive opens, descriptor lifetime, capture integrity, wrong devices, bad
pointers, ioctls and seek at 16/128 MiB. `make test-output-unit` injects first and
later storage failures and checks sticky error, short-write and inode cleanup.

# Printer/plotter endpoints (task 77)

`/dev/vp` (30:1) and `/dev/vs` (30:2) use the same explicit empty-file attachment,
pinning and error policy. Both are root-owned mode 0600. They replace reference
`vp.c` and `vs.c`; native captures replace unavailable Versatec DMA and DC-11
request/clear-to-send hardware. The synchronous sink is always transport-ready;
filesystem errors replace unavailable/offline sink errors. Hardware register
images and wire timing are not emulated.

VP is exclusively opened write-only. VGETSTATE (`'v'<<8`) and VSETSTATE
(`('v'<<8)+1`) transfer native uint16_t state, preserving reference mode bits
PRINT 0100, PLOT 0200, PPLOT 0400 and command bits 076. Get includes ISOPEN 01;
commands are synchronous, so no pending command/BUSY flags remain afterward.
Set masks mode and command bits as the source does. Writes split into at most
512-byte DMA-equivalent records; exact PPLOT mode changes to PLOT after the
first successful record.

A VP capture starts with eight bytes `NXVP`, byte 1, and three zero bytes.
Every record has an eight-byte little-endian header: uint16_t type, uint16_t
mode-or-command, uint32_t payload length. Type 1 records commands with zero
payload; type 2 contains exact data and the mode used. Attachment captures the
open-time RESET 02, CLRCOM 04 and FFCOM 020 in ascending order. VSETSTATE emits
command bits in ascending order too. This framing preserves control/data
boundaries for later renderers; it is a Nexix transport format, not historical
printer input bytes. No implicit rendering or form-feed conversion occurs.

VS allows multiple opens, including read/write. Every open and close emits GO
026; each write emits GO before its first data byte, before every subsequent
120-byte group, and after its final byte, following reference vswrite. A capture
attached after multiple opens receives their pending GO markers. Duplicated
file descriptors trigger close handling only at final description close.
Each description close clears shared input, matching the reference; the capture
pin is released after the last independent open.

`NX_VS_INPUT` supplies native input codes through a 16-byte record (uint32_t
count, eleven uint8_t codes, one zero reserved byte). It is an explicit local
peer/test transport, not a historical ioctl. Queue capacity is eleven, matching
the reference `c_cc <= 10` insertion condition; overflow returns ENOSPC atomically.
Reads return one translated byte using the source `?0*#?546?213?879?` low-nibble
table. Empty reads sleep interruptibly; zero-size I/O does nothing. Peer injection
requires a descriptor, so normal device permissions control access.

`make test-printer` verifies captured commands, PPLOT chunk transitions, bad
ioctl pointers, VS burst boundaries, all queue slots and input translation at
16/128 MiB. Shared host tests cover failed VP initialization, VS multiple-open
pin lifetime, interruptible empty input and failed close cleanup. The original
C/A/T fixture remains a regression test.

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

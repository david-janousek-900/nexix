# Nexix working sources for Traditional Vi 050325

These are native working copies of the pinned release in
`reference/ex-vi/050325`; copyright/license notices remain in each source.
Provenance and release archive hashes are in `reference/ex-vi/provenance.json`.
Do not edit the preserved reference to change the port.

Task 379 supplies native compilation and linking, with mandatory diagnostics for
missing declarations, incompatible pointers and pointer/integer conversions.
The editor keeps its bundled historical regexp engine, pool allocator and private
printf/getchar/putchar implementation. Editor I/O is namespaced so libc stdio
continues to use its own FILE interface. No host libc or Linux headers are used. LONG_BIT=64 selects full-width decimal
buffers; hexadecimal/octal masks and signed magnitude conversion preserve native
long values, including LONG_MIN and ULONG_MAX.

The signal bridge reinstalls handlers using V7 signal() before dispatch, retaining
persistent editor handlers without claiming POSIX sigaction/masks. Upstream
SIGWINCH/job-control blocks remain disabled because those signals do not exist on
Nexix. Nonlocal exits use native setjmp/longjmp; no signal mask exists to restore.
Filesystem transfer sizing uses V7 BSIZE rather than adding a fictional stat
field. The write-completion call uses native sync; error paths use Nexix's checked
error table. Local/home directory identity uses device/inode comparison, avoiding
a dependency on getcwd. Function-key description storage remains live through
addmac's copy.

`make test-vi-native` checks repeated signal dispatch, old dispositions, error and
directory adapters, plus linked ex startup/quit on private 16/128 MiB guests.
Those startup tests create private /var/tmp and an inline dumb terminal entry;
production storage, interactive operations, recovery and installation have their
own following tasks. The development editor ELF is not installed by this task.

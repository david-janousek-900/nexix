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
Those startup tests use an inline dumb terminal entry; interactive operations,
recovery and editor installation have their own following tasks. The development editor ELF is not installed by this task.


Task 380 installs the /var/tmp directory and keeps the native line references
separate from the initial 32 KiB libc allocation pool. Pool growth is checked
before arithmetic; failure of the initial reservation exits without jumping to
an uninitialized error context. Temporary-name allocation and seeks are checked.
Existing exclusive-create names and mode 0600 protect pre-existing candidates;
storage failures retain a nonzero exit status through batch q!.

`make test-vi-storage` runs the real editor as uid 100, loads 6,000 lines (528 KiB),
checks a live temporary file's owner/mode and name-collision behavior, saves and
compares every byte, then fills the disposable filesystem and checks an ENOSPC
error, nonzero exit, removal of the editor's temporary file and unchanged saved
content. It runs at 16/128 MiB. V7 directory permissions remain V7 permissions;
this task does not introduce POSIX sticky-directory semantics.

Task 381 integrates ex command execution. Batch command errors retain failure
status through a subsequent quit; the database includes a dumb terminal entry
for the upstream `ex -` batch mode. `make test-ex` checks append, substitution,
global deletion, file reads/writes, argument-file reopening, byte-for-byte file
comparison, shell escapes and invalid-command/no-match/write failures at
16/128 MiB. It checks both result text and printed command output.

Task 382 enables native visual vi/view. The BIT8 screen buffer is bounded for the
fixed native terminal sizes and fits the 32 KiB user stack (the release's default
16,000 short cells plus scratch space would exceed it). The QMP keyboard harness
sends a real Escape key. `make test-vi` exercises serial and physical VGA memory
with PS/2 input: insert, line motion, deletion/single undo, search, save/reopen,
read-only write rejection, unchanged view contents and restored shell modes at
16/128 MiB. No modern vi extensions or UTF-8 behavior are implied.

Task 383 ports the release preservation/recovery helpers over private V7
fixed-record directory adapters. Header checks bound line counts, names and block
locations and require regular files whose owner matches the recorded uid.
Preserved files use mode 0600 in root-owned /var/preserve (0755); only expreserve
needs setuid root, while exrecover runs as its caller. Recovery/helper failures
and signal deaths propagate failure to ex. Native qsort is a separate libc
archive member so recovery can retain the release's sbrk/pool allocator.

`make test-vi-recovery` edits as uid 100, sends SIGHUP, checks private ownership,
boots the same disposable disk again, lists saved buffers using vi -r, rejects
uid 101, recovers every byte as uid 100 and rejects malformed input at 16/128 MiB.
If /bin/mail is absent, preservation skips the optional mail notification; it
still saves the buffer successfully. When mail is installed, the release's
notification path remains available with a controlled MAILRC environment.

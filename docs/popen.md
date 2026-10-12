# Process pipes (task 49)

Native `stdio.h` declares `popen(const char *, const char *)` and
`pclose(FILE *)`, exported by `libc.a`. Their reference is
`usr/src/libc/stdio/popen.c` and `usr/man/man3/popen.3s` in the preserved V7 tree.

`popen(command, "r")` returns a stream reading the command's standard output.
`popen(command, "w")` returns a stream writing the command's standard input.
The child executes `/bin/sh` with `sh`, `-c`, and the command, inheriting the
caller's environment, cwd and unrelated descriptors. Only the literal modes
`r` and `w` are accepted; NULL arguments and other modes return NULL/EINVAL.
An empty command is valid. This is a one-way pipe, not an update stream.

`pclose` closes the stream, flushing pending output before waiting so that the
command receives EOF, and returns the associated shell's **raw wait status**.
Interpret it with the installed wait macros, as for [system](system.md).
The caller ignores SIGINT, SIGQUIT and SIGHUP during collection, then restores
all three dispositions. Child dispositions are inherited before those changes.
SIGPIPE is unaffected: a caller writing to an exited command needs the usual
V7 handling for that signal.

## Ownership and native corrections

Each successful popen records its FILE, descriptor and child PID in a bounded
twenty-entry table, matching the native stream limit. pclose accepts only a
currently owned stream. NULL, an ordinary stream and a repeated pclose return
`-1`/EINVAL without closing unrelated streams or waiting for arbitrary children.
Use pclose rather than fclose/freopen or raw close on a popen stream; independently
closing/reusing its FILE or descriptor violates that ownership contract.

The implementation fixes several source-level cleanup and concurrency gaps:

- Allocate the FILE before fork. Stream allocation failure closes both pipe
  ends without creating a child; fork failure closes the stream and other end,
  preserving the original errno.
- Close the parent's pipe end in the child and the child's pipe end in the
  parent. Preserve the duplicated descriptor when its source already equals
  stdin or stdout, rather than closing that standard descriptor afterward.
- Close inherited descriptors for other active popen streams in a new command
  child. They otherwise keep unrelated pipe writers alive and prevent EOF.
- Cache another open popen command's status if wait-any collects it on the way
  to the requested child. Its later pclose returns the cached status and closes
  its stream without waiting for a different process.
- Remove an ownership entry exactly once, close its stream on wait failures and
  preserve wait errno across signal restoration.

This uses V7 wait-any; it does not add waitpid to the kernel ABI. Unrelated
ordinary children can still be reaped and their statuses discarded, as in the
reference. Calling wait or system while popen children are outstanding can
likewise consume their statuses; pclose then reports ECHILD. On EINTR, pclose
returns `-1`, closes the stream, restores dispositions and leaves the child for
a later explicit wait. It does not retry interrupted waits or kill children.

No existing stdio buffers are flushed before fork. As the V7 manual notes,
buffered stdin reads can advance the shared descriptor beyond data still held
in the caller's buffer, and pending output should be explicitly flushed before
using a command as a filter on that descriptor.

As in the source, pclose returns a collected child status even if fclose's
flush failed; the native implementation retains that close errno when wait
succeeds. Check fflush before closing if output-delivery failure must be
distinguished from child exit. A wait error takes precedence over a close error.

Child dup/exec failure uses `_exit(1)`, without flushing copied buffers or
running exit callbacks. The supplied manual suggests shell-access failure can
produce NULL, but its implementation has no parent/child exec handshake:
successful setup returns a stream and pclose observes `1 << 8`. Nexix follows
that supplied source behavior. This differs from system's `_exit(127)`.

## Verification

Run `make test-popen`.

- Host fixtures inject pipe, FILE allocation, fork, dup and exec failures;
  validate child argv and descriptor direction; check setup rollback, raw
  statuses, signal restoration, unrelated waits, cached sibling statuses,
  EINTR/ECHILD, flush errors, invalid closes and repeated ownership reuse.
- Native V7 acceptance boots at 16/128 MiB exercise output reading, buffered
  input writing, direction errors, nonzero/signal statuses, concurrent streams,
  EOF across inherited pipes, descriptor exhaustion, broken-pipe/ECHILD cleanup
  and pipe endpoints occupying closed stdin/stdout.
- Existing system/shell tests, public declaration/export checks and the host
  unit suite remain enabled. Test roots are disposable; persistent user storage
  is preserved.

These functions retain static, single-threaded V7 process semantics. Their
addition completes this libc task, not the remaining kernel/libc or consumer
program ports.

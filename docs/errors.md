# V7 error messages and perror (task 43)

`libc/errors.c` preserves all 35 messages (indexes 0–34) from
`reference/v7/usr/src/libc/gen/errlst.c`, including their spelling and order.
For example EPERM is `Not owner`, EBADF is `Bad file number`, ENOTTY is
`Not a typewriter`, EDOM is `Argument too large`, and ERANGE is
`Result too large`. This is the V7 message table, not the host libc's text.

Nexix's existing ENOSYS=35 adds `Function not implemented`. Consequently
`sys_nerr` is 36. `sys_errlist` is an exported array of char pointers and
`sys_nerr` an exported int, matching the original interfaces. They are declared
in both native errno.h and stdio.h. Programs must check
`errno >= 0 && errno < sys_nerr` before indexing. String contents reside in
native read-only data; pointer entries retain the historical mutable interface.

`libc/perror.c` replaces the former numeric `error N` implementation. It uses
V7's direct writes to descriptor 2, independent of the global stderr pointer,
stream buffering and FILE error flags. With a nonempty prefix the output is
`prefix: message\n`; empty or NULL prefixes emit just `message\n`. Negative,
out-of-range or unavailable indexes use `Unknown error`. Lookup is also bounded
by the actual native table even if exported sys_nerr is increased by a caller.

The native adaptation preserves errno across successful or failed diagnostic
writes, retains the existing NULL-prefix convenience, and handles negative
error numbers without the original negative-array indexing. It retries EINTR
and advances across short writes; other failures or zero-length progress end
that write attempt. It does not change signal dispositions: a broken pipe can
still deliver SIGPIPE. It does not flush, reorder or set error flags on stderr's
userspace buffer. For example pending buffered text may appear after a direct
perror message when the stream is later flushed, as with the V7 write-based
implementation.

The reference source files and `usr/man/man3/perror.3` are unchanged. The native
public perror returns void; this task does not introduce strerror or change
kernel errno numbers, syscall ABI versions, or component completion claims.
Libc remains partial, with password/group, crypt and other interfaces still
assigned to their roadmap tasks.

## Verification

```sh
make test-perror test-unit test-inventory
```

A golden-source check compares every original message with the native table and
checks contiguous kernel errno numbering plus the sole ENOSYS extension. Host
write fixtures test exact prefixes/text/newlines, error zero, unknown negative/
large numbers, mutable sys_nerr bounds, errno preservation, EINTR, partial writes,
failed writes and zero progress.

Native tests at 16/128 MiB capture actual descriptor-2 output through pipes. They
verify a NULL stderr pointer cannot redirect perror, buffered text remains
pending until explicitly flushed, and closed/broken descriptors preserve errno
without setting FILE error state. A rebuilt V7 ln command is exercised through
its actual failed-link path and shell stderr redirection, requiring exactly
`ln: File exists\n` and exit status 1. Existing stdio/signal/process/filesystem
and cleanup tests run in the same boots.

The interface export checker now requires sys_errlist/sys_nerr along with the
existing public data symbols. The original consuming programs using their own
error-table formatting (for example time, nice and kill) remain with their
individual port tasks; table availability does not mark those programs ported.

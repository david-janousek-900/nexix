# Terminal and login lookup interfaces (task 47)

`getpass`, `ttyname`, `ttyslot` and `getlogin` are declared in the native
`unistd.h` and exported from `libc.a`. Their original sources are
`reference/v7/usr/src/libc/stdio/getpass.c` and
`reference/v7/usr/src/libc/gen/{ttyname,ttyslot,getlogin}.c`.
The reference remains unchanged. Login, account configuration and authentication
programs retain their separate roadmap tasks.

## Password input

`getpass(prompt)` opens `/dev/tty` as an unbuffered input stream, falling back
to `stdin` if the open fails, as in V7. It ignores SIGINT during the operation,
clears ECHO, prints the prompt to stderr, consumes input through newline or
EOF and prints a newline. The returned static nine-byte buffer holds at most
eight input bytes and a terminating NUL. Excess input is consumed; the next
call reuses the same buffer. Clean EOF returns the accumulated password,
including an empty string. Input is byte-based, not Unicode character counting.

Native adaptations check terminal setup before reading, write the prompt
literally instead of treating it as a printf format, and flush stderr so a
buffered prompt is visible. A NULL prompt is EINVAL. A non-terminal fallback
fails with ENOTTY rather than reading an echoed password with uninitialized
terminal settings. Read/output/setup failures return NULL and retain their
errno. On every recoverable path the function attempts to restore the saved
terminal settings and SIGINT handler, then closes its private stream. It never
closes stdin. A restoration or close failure also returns NULL; an earlier
error retains precedence. The static password is cleared on failure.

These functions retain V7's static-storage, single-threaded model. getpass
does not install handlers for every terminating signal; default SIGQUIT or
process termination retains the historical behavior. Restoration attempts
cannot guarantee success after the terminal disappears or an ioctl fails.

## Terminal identity and slots

`ttyname(fd)` first requires a terminal and character-device inode. It scans
V7's sixteen-byte `/dev` directory records, matching inode and filesystem
device, then returns the first matching pathname in static storage. It does
not manufacture a name from the device number or the controlling-terminal
alias. Thus hard-linked `/dev/console` and `/dev/ttyS0` can resolve to
`/dev/console` according to directory order. A full fourteen-byte directory
name is copied with an explicit NUL, avoiding the original strcat assumption.
No match returns NULL with ENOTTY; syscall failures preserve their errno.

`ttyslot()` tries descriptors 0, 1 and 2 in that order. It matches the basename
against `/etc/ttys` names following two control bytes and returns the one-based
physical line number. Disabled entries and malformed lines still occupy slots;
the first matching terminal wins. Missing terminals, unreadable files and no
match return zero. As in the supplied V7 reader, a final unterminated line is
not an entry. Native parsing bounds lines to 31 bytes, drains oversized lines
and skips malformed/oversized entries instead of overflowing the original
32-byte buffer. Interrupted reads retry; short record reads accumulate.

## Login records

`getlogin()` selects `/etc/utmp` record `ttyslot()`; record zero is reserved.
It reads the installed native 24-byte `struct utmp`, with eight-byte line and
name fields and a 64-bit time field. Future init/login/who writers must use this
same ABI rather than the historical PDP-11 record layout. Preserved
PDP-11 records need explicit conversion before native use.

The name is copied into a separate static nine-byte buffer and terminated at
the first space, NUL or eight-byte boundary. This preserves V7's space trimming
without overwriting the timestamp adjacent to `ut_name`. Empty names return
an empty string. Missing slot/file, failed seek/read or incomplete records
return NULL. Every opened descriptor is closed, including short-read paths
where the supplied getlogin leaked its descriptor. These lookups do not consult
environment variables or claim that the caller is authenticated.

## Acceptance

Run `make test-terminal`. It includes:

- Host fixtures for full-width directory/name fields, device mismatch,
  descriptor fallback, malformed/oversized tty lines, partial records,
  interrupted reads and injected permission/read/seek failures.
- Host getpass fixtures for literal prompts, truncation, excess-input draining,
  EOF, stdin fallback, static reuse and setup/read/output/cleanup failures.
- Native filesystem fixtures in the existing 16/128 MiB acceptance boots:
  tty identity, slot-zero exclusion, disabled-line numbering, real utmp layout,
  full/space-padded names, repeated short reads without descriptor leaks and
  permission denial in a child with reduced credentials.
- Real QEMU COM1 input at 16/128 MiB: eight-byte truncation, ignored Ctrl-C,
  handler/terminal restoration, repeated input and Ctrl-D. The harness checks
  that the password bytes never appear in serial output.
- Public declaration/export checks and existing TTY/signal/stdio regressions.

All filesystem mutations occur in disposable test roots. The working persistent
disk is preserved.

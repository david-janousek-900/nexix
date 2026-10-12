# Shell command execution (task 48)

`system(const char *command)` is declared in native `stdlib.h` and exported
from `libc.a`. It adapts `reference/v7/usr/src/libc/stdio/system.c`, with
the contract described in the supplied `usr/man/man3/system.3`.

The child executes `/bin/sh` with arguments `sh`, `-c`, and the command text,
using the caller's environment, working directory and inherited descriptors.
The shell path is fixed; PATH does not select it. Normal exec rules still apply
to close-on-exec descriptors. An exec failure uses `_exit(127)`, avoiding stdio
flushing and exit callbacks copied from the parent.

The parent ignores SIGINT and SIGQUIT while waiting, then restores the previous
dispositions. Fork happens before those changes, as in the original, so the
child inherits the caller's original signal dispositions. The V7 shell then
applies its own signal handling, and exec retains ignored signals while
resetting caught handlers under the existing kernel contract. No extra signal
mask or POSIX child-handler reset is added.

The return value is the **raw wait status**, not the extracted exit code:

| Outcome | Return value |
|---|---|
| Shell exits 0 | `0` |
| Shell exits 7 | `7 << 8` |
| Shell cannot execute | `127 << 8` |
| Shell terminates from a signal | Native V7 wait status, including signal/core bits |
| Fork or wait fails | `-1`, with the syscall errno |

Use the installed `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED` and `WTERMSIG`
macros to interpret successful status collection. Exit 127 can also be returned
by an executed shell; it does not uniquely establish an exec failure.

## Historical behavior and native adaptations

V7 provides wait-any rather than waitpid. `system` loops until it collects its
shell or wait fails; another child's status may be consumed on the way. It
does not preserve those unrelated statuses for a later wait. This follows the
supplied implementation and remains relevant to callers managing other children.

An interrupted wait returns `-1` with EINTR, restores the parent dispositions
and leaves the shell available for a later wait. It does not silently retry
EINTR or terminate the child. Signal changes are process-wide and follow the
current single-threaded V7 process model.

Two checked adaptations avoid accidental side effects: a NULL command returns
`-1` with EINVAL without forking, and fork failure returns immediately without
waiting for unrelated children or obscuring the fork errno. An empty string is
a valid shell command. NULL is not the modern POSIX shell-availability query;
the supplied V7 manual does not specify that query. Wait errno is preserved
across signal restoration. The source tree and manual remain pristine.

## Verification

Run `make test-system`.

- Host syscall fixtures verify exact child exec arguments, `_exit(127)`, fork
  before signal changes, ignored signals during wait, restoration, raw exit and
  signal/core statuses, wait-any collection, EINTR/ECHILD, and fork failure.
- Native 16/128 MiB V7 acceptance boots exercise the real shell, empty commands,
  exit values 0/7/255, signal termination after shell exec, inherited ignored
  versus caught SIGINT, restored caller handlers, inherited environment and cwd,
  file redirection, and repeated invocation without outstanding children.
- Public declaration/export checks and the existing V7 shell/utilities fixtures
  run with the new interface.

Native command tests use the existing embedded-program fixture; host injection
provides the missing-shell and syscall-error cases. No accounts, shell program
ports or persistent disk contents are changed by this task. Core file generation
remains task 52; preserving a wait-status bit does not implement core dumps.

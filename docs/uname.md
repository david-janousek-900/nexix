# System identity

`/bin/uname [-asnrvm]` reports the running kernel's identity. Without options,
it prints `Nexix`. Flags select fields in the following order, regardless of
option order; duplicates print a field once. `-a` selects all five fields.

| Flag | Field | Current value |
| --- | --- | --- |
| `-s` | Kernel name | `Nexix` |
| `-n` | Node name | `nexix.localdomain` on fresh installations |
| `-r` | Kernel release | `0.1.0` |
| `-v` | Kernel version | `development` |
| `-m` | Machine architecture | `x86_64` |

Options can be combined or separate. `--` ends options; operands and unknown
options return 2 with usage on stderr. Successful output returns 0; system or
output errors return 1. Output is space-separated and ends with a newline.

This is a native addition, not a port of a V7 command. It queries the running
kernel through the native `uname(struct utsname *)` libc interface, declared
in `<sys/utsname.h>`. Native syscall 74 copies five fixed, NUL-terminated
65-byte fields to the caller, returning zero or negative errno. Libc converts
errors to -1/errno. Invalid, unmapped or read-only destinations fail with
EFAULT. The query requires no privilege and does not read the host system.

`kernel/include/version.h` owns the reproducible kernel identity and fallback
node name. Init loads the full configured node name from `/etc/hostname` before
starting sessions; root can change it with [`hostname`](hostname.md). Compile
timestamps are not included. Both the public header and shared record layout
are installed in the full distribution's development headers.

`make test-uname` runs sanitizer tests for flags and failures, then boots the
installed command and API fixture at 16 and 128 MiB as root and Alice. The API
checks identity, destination guards, errno and read-only/unmapped addresses.

# Multi-user implementation

Root's home is `/root`, owned by root with mode 0700; its login profile is
`/root/.profile`. Native 115200 getty sessions use DEL/Backspace (also Ctrl-H in
cooked mode) to erase and Ctrl-U to kill a line. Login preserves these settings,
including when VGA is named `/dev/console`. Historical terminal profiles retain
their configured erase/kill characters. See [kernel messages](kernel-log.md) for
uptime prefixes and `dmesg`.

The independent terminal setup below uses the explicit `console=multi` profile
(`make multi-iso`, `build/nexix-multi.iso`). Normal boots now select only VGA,
or COM1 with `make run-serial`; see [console selection](vga-console.md).

Task 94 prepares the full IDE installation's V7 account and startup files.
`/etc/passwd` uses the traditional seven fields and DES password hashes;
`/etc/group` uses V7's name/password/GID/member-list records. Both files are
root-owned and mode 0644. There is no shadow-password database or supplementary
group API. Historical service identities daemon, sys, bin and uucp are reserved
with locked password fields, alongside root and two development users.

Fresh development disks provide these login credentials when login is installed:

| User | Password | UID:GID | Home |
|---|---|---|---|
| root | nexix | 0:0 | / |
| alice | alice | 100:100 | /home/alice |
| bob | bob | 101:100 | /home/bob |

The two user homes are mode 0700 with their respective ownership. Their mode-0644
profiles set PATH and TZ, preserving the HOME supplied by login. The shared V7
`/tmp` is mode 0777 for unprivileged temporary files. Root retains its
existing profile. TZ defaults to `GMT0`; native time-zone handling is documented
in [timekeeping](time.md). `/etc/motd` identifies the development system.
`/etc/rc` runs startup commands; init supervises terminal processes. `/etc/ttys`
now enables both native lines for automatic multi-user startup (task 101).
Bootstrap configuration remains the fixed single-user recovery
installation with its original password-less root account and empty ttys file.

Reference configuration is preserved in `reference/v7/etc/passwd`, `group`, `ttys`
and `rc`. Native accounts, password hashes, home paths and line names are explicit
installation policy rather than copied historical host identities. Native libc
and the retained V7 commands read the original database formats.

`make test-account-config` boots disposable IDE roots at 16/128 MiB. Native libc
looks up the installed users/groups and verifies the DES hashes; forked children
drop credentials, create owned files in their own homes, read the account data,
and fail to access another home or write `/etc`. Staging and bootstrap separation
checks verify the installed modes/owners without host chown or device nodes.

Task 95 installs `/bin/stty` from `reference/v7/usr/src/cmd/stty.c`. Its mode,
erase/kill, speed and reporting tables remain source-based. The native port adds
115200 to both the accepted-speed and reporting tables; the latter must cover
Nexix's additional speed index. It reports failed terminal operations and missing
erase/kill operands instead of reading past argv or reporting uninitialized data.
Unknown modes produce the original diagnostic and a deterministic failure status.
The PC UART uses one rate for input/output; unsupported unequal-rate requests
fail through the existing TTY contract.

`make test-stty` executes the installed command on real guest terminals at
16/128 MiB. It checks 115200/9600 reporting, erase/kill, raw and echo changes on
COM2 while COM1 settings remain unchanged, plus missing operands, unknown modes
and a regular-file descriptor. Both stderr and exit status are checked.

Task 96 installs `/bin/tty` from the preserved V7 `usr/src/cmd/tty.c`. Its behavior
is unchanged: identify fd 0, print `not a tty` and exit 1 when identification
fails, and suppress output with `-s` while retaining that status. The native
stdio declaration is included explicitly. `make test-tty-command` uses the
component guest harness at 16/128 MiB to check COM1 (`/dev/console`), COM2
(`/dev/ttyS1`), output through a pipe, regular-file/pipe input, and silent mode.

Task 97 installs V7 `/bin/mesg`. It identifies fd 2, tests the terminal inode's
other-write bit and applies the source's 0622/0600 modes. A negative answer or
`mesg n` exits 1; invalid usage and failures retain exit 255. The port explicitly
checks ttyname failure before stat. `make test-mesg` checks both lines, independent
modes, non-TTY/error paths, and a real UID-100 process failing to chmod a root-owned
terminal that it inherited open. V7's message permission semantics are retained.

Task 98 installs `/etc/getty` from V7 `usr/src/cmd/getty.c`. Its speed-table loop,
username editing, case detection and login exec remain source-based. Profile `9`
adds 115200 baud with native Ctrl-C, raw username input and cooked/echo login
handoff. Other profile entries retain their historical settings and source
software parity in the raw username protocol. Native profile 9 sends clean ASCII;
the final acceptance also checks the historical 9600 baud profile and its parity.
The private output routine is renamed to avoid interposing libc puts. The username
buffer reserves a terminator after the original 16-character bound; ioctl failures
exit rather than continuing with an unconfigured line.

`make test-getty` uses an explicitly private `/bin/login` stand-in on a disposable
IDE root. It verifies argv and cooked settings at the exec boundary, clean prompts,
erase/kill, uppercase input, the full name bound, empty input, zero-byte speed
cycling, EOT exit and both UARTs. Authentication is implemented separately in task
99; production init supervision and respawn are task 101.

Task 99 installs V7 `/bin/login`: DES password checking and retry, home selection,
tty ownership, UID/GID transition, HOME/PATH, umask, motd and login-shell exec.
The port uses the native setpwent declaration, terminates the eight-byte login
name before string lookup, checks password/credential errors, and maps the source
nice reset to the native bounded increment. Failed shell exec exits unsuccessfully.
The missing V7 `execlp` libc entry point is supplied with native varargs and the
existing PATH-search implementation; login's bare-shell fixture exercises it.

`make test-login` checks wrong/unknown passwords without echo, retries, a missing
home, and a private shell's real/effective IDs, cwd, HOME/PATH, tty ownership and
umask. It then starts the real V7 user shell in Alice's home and checks clean
logout and username EOF. Runs use disposable IDE volumes at 16/128 MiB. Source
login record writes are present; concurrent accounting and logout integration
are accepted in task 100. Automatic getty supervision remains task 101.

Task 100 shares native record-I/O adapters between login and init's source-based
`rmut` path. `/etc/utmp` uses one-based tty slots (slot zero is empty), while
`/usr/adm/wtmp` retains an append-only sequence of login records and empty-name
logout records. Each record is 24 bytes: line[8], name[8], and a signed 64-bit UTC
epoch time. Fields are padded/truncated to eight bytes; there is no byte-for-byte
PDP-11 accounting compatibility. History is preserved across init startup; active
utmp is cleared once at boot, rather than on every ttys merge. Logout only clears
matching line records, preserving other active users.

Record I/O handles short/interrupted reads and writes. Wtmp uses O_APPEND to avoid
the source's separate seek/write race between line processes. Missing/unwritable
accounting files retain V7's best-effort login behavior; this is not a transactional
database or a guarantee against partial records after an I/O failure. Init still
records line-process exits even if no user logged in, as the source does.

`make test-session-accounting` enables two gettys through the existing manual
single-user transition, authenticates Alice and Bob, and checks the actual native
utmp slots, wtmp sequence, RTC times and file mtimes. Alice's logout must preserve
Bob's session; root then logs in and Bob logs out. Both lines respawn getty. Runs
use disposable roots at 16/128 MiB. Automatic startup and reload/recovery acceptance
follow in task 101.

Task 101 boots a fresh full IDE installation through rc directly to `login:` on
COM1 and COM2. `/etc/ttys` contains `19console` and `19ttyS1`: enabled flag `1`,
native 115200 profile `9`, and the device basename. Empty/disabled configuration
retains the single-user path, including the fixed RAM bootstrap. Logging out or
sending EOT to getty starts a replacement prompt.

V7 init still owns its table, rc execution, wait/reap loop, ttys merge and recovery
path. Native changes retry interrupted wait after SIGINT reload, establish each
line's own group, clean up remaining members of an exited/disabled line's group,
and check line setup/exec failures through a close-on-exec readiness pipe. Normal
shell exit codes (including 126/127) still respawn. An unavailable line is retried on the next
reload rather than spinning; failed getty setup on all lines leads to recovery.
Only PID 1 may execute init's supervisor path.

Root can signal PID 1: SIGINT merges terminal configuration, preserving unchanged
sessions and their utmp records; SIGHUP stops sessions and opens the console
single-user recovery shell. Ctrl-D then runs rc and restores configured gettys.
The historical `kill` command is still tracked by task 126; acceptance uses a
private signal helper rather than claiming that command is installed.

`make test-multiuser-init` checks automatic two-line login, process groups,
unchanged-session reload, disable/enable, recovery, return to multi-user, logout
and getty EOT at 16/128 MiB. Existing shell/storage tests use explicitly generated
single-user fixtures; the installed seed remains multi-user. Existing working
disks keep their configuration. To preserve one while testing a fresh seed, use
an unused path with `make DISK_IMAGE=build/fresh.img run`.

Task 102 installs V7 `/bin/passwd` set-ID root (04755). It keeps real-UID account
authorization, old-password checks, strength/confirmation prompts, DES salts and
the original command's exit status 1, including after successful updates. Native
declarations and failed password-input/stdio operations are checked.

The native update path acquires `/etc/ptmp` with O_EXCL, writes and closes a full
candidate, and publishes it using V7 link/unlink operations. A temporary
`/etc/opasswd` hard link preserves the original while its directory name changes.
Failed publication restores the original; failed rollback retains both recovery
copies and reports the backup path. An existing backup is never overwritten.
Successful updates restore root:root 0644 and remove lock/backup names. Syncs bound
the publication stages, but the two name-changing operations are not an atomic
rename. An interrupted update may require root to inspect `/etc/opasswd` and
`/etc/ptmp` before restoring a missing `/etc/passwd` with the retained `ln` command.

`make test-passwd-command` injects failed backup, unlink, publish and rollback
operations around real isolated host files. Guest runs at 16/128 MiB exercise
non-owner denial, wrong old passwords, confirmation mismatch, lock/backup
conflicts, ordinary updates, root reset, final metadata and authentication with
the changed password. Plaintext input remains behind getpass's echo suppression.

Task 103 installs V7 `/bin/su` set-ID root. It preserves the source's optional
username (default root), DES prompt, real-root bypass, target UID/GID and shell,
and inherited HOME/environment/cwd. V7 has no `su -` login-environment mode; the
roadmap's environment acceptance checks that original preservation behavior.
The port checks password-input and credential failures and uses a native null
vararg terminator. `make test-su` checks wrong/unknown credentials, hidden input,
root bypass, real/effective IDs, inherited HOME/cwd, nested shells and failed exec
at 16/128 MiB.

Task 104 installs V7 `/bin/newgrp` set-ID root. Membership and the special `other`
group follow the source. Its historical group-password branch applies when a
member's account has an empty password. Denial and usage errors still replace
the shell with unchanged credentials, rather than returning a modern failure
status. The port checks password/UID-drop errors and closes all native extra
descriptors 3–19. The V7 shell's `newgrp` builtin uses exec, so the replacement
keeps the login PID/group and inherited HOME/cwd.

`make test-newgrp` checks membership, `other`, denial, unknown group, usage,
real/effective IDs, inherited environment, closure of an inherited fd 19, and
wrong/correct group passwords for a private blank-password account. Tests use
actual login and builtin dispatch at 16/128 MiB.

Task 105 installs V7 `/bin/who` with its normal, explicit-file and three-argument
(`who am i`) modes. It reads the native 24-byte records, retains source formatting
and empty-record behavior, bounds fixed-width field comparisons/copies and checks
time/stream errors. `make test-who` checks two live sessions, each terminal's
identity, no-TTY fallback, explicit utmp and nonterminated eight-byte records,
missing files and logout at 16/128 MiB.

Task 106 installs V7 `/bin/write`, retaining utmp lookup, explicit tty selection,
source headers/bells, message permission checks, local `!` shell escapes and the
`EOF` termination marker. Native handlers use the signal ABI; fixed-width records
and tty basenames are bounded, device type/mode is checked before opening, format
strings are explicit, and a 128-byte escape reserves its terminator. Native write
errors are checked. `make test-write` exchanges real messages between Alice and
Bob at 16/128 MiB, checks mesg denial/enable, invalid names/usage, explicit tty,
the full escape-buffer boundary, EOF and Ctrl-C, then verifies both shells live.

Task 107 installs V7 `/etc/wall`. It retains file/stdin input, the 50-record table,
per-recipient sleep/fork, source broadcast header and asynchronous child delivery.
The parent retains status 0 even if a delivery child reports failure. Ordinary
senders respect device permissions; root can broadcast to a `mesg n` terminal.
The port bounds the original 3000-byte message buffer, checks input/output errors,
and validates fixed-width terminal paths and device type before opening.

`make test-wall` checks file/stdin delivery to both authenticated users, ordinary
and root permissions, exactly 3000 bytes, oversize/missing input and a malformed
utmp entry naming a regular file. That file must survive unchanged. Runs at
16/128 MiB wait for each asynchronous delivery and verify both shells survive.

Task 108 completes the multi-user milestone. `make test-multiuser-acceptance`
copies the exact production seed using the same non-clobber disk publisher as
`make run`, then boots two authenticated terminals at 16/128 MiB. It exercises
wrong/correct hidden passwords, permissions, messaging, passwd/su/newgrp,
per-terminal flow control, empty lines and logout. Host decoding independently
checks persisted UID/GID/modes, contents and V7 timestamps. A second cold boot
uses the next day's RTC, authenticates the changed password and reads the saved
files; reseeding must preserve the changed disk, and mtime must remain intact.

A separate disposable fixture uses a private kinfo observer with the production
init/getty/login to compare process/file/inode/text counts and free physical pages
across four logout/login cycles. The exact-production-seed tests install no
private observer. Working-disk hashes are checked before and after acceptance.
Final source review also closes the getty profile parity split: native profile 9
stays clean, and a legacy profile handoff checks the original even-parity bytes.
Terminal and QEMU logs use `build/multiuser-*`; the private observer logs use
`build/multiuser-resources-*`.

`make test-multiuser` runs all terminal, account, command, init, messaging and final
multi-user acceptance targets. This completes tasks 86 and 89–108; it does not
declare the complete V7 distribution port or version 0.1.0 finished. Remaining
ports and release work stay on the [task list](tasks.md).

# Multi-user implementation

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
profiles set PATH and TZ, preserving the HOME supplied by login. Root retains its
existing profile. TZ defaults to `GMT0`; native time-zone handling is documented
in [timekeeping](time.md). `/etc/motd` identifies the development system.
`/etc/rc` runs startup commands; init supervises terminal processes. `/etc/ttys`
currently declares both lines disabled until getty/login and automatic multi-user
startup are ready. Bootstrap configuration remains the fixed single-user recovery
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

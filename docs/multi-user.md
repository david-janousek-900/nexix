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

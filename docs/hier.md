# File system hierarchy

Nexix keeps V7's tree and follows the 4.4BSD rule for system programs (tasks
432–434):
- programs live in the binary directories;
- `/etc` holds only configuration and rc scripts.

`make test-etc-layout` enforces both rules, on the installation manifests and
on the built distribution image, and `make test-layout-upgrade` enforces them
on upgraded disks.

| Directory | Contents | Examples |
|---|---|---|
| `/bin` | Ordinary user commands (Nexix keeps V7's single `/bin` for them) | `sh`, `ls`, `cc`, `vi`, `ping`, `nc` |
| `/sbin` | System programs needed to boot and repair, before `/usr` | `init`, `dmesg`, `mknod`, `ifconfig`, `route` |
| `/usr/bin` | User commands that BSD keeps under `/usr` | `wall` |
| `/usr/sbin` | Administration programs not needed for booting | `accton`, `update` |
| `/usr/libexec` | Programs started by other programs, not by users | `getty`, `expreserve`, `exrecover`, `manfmt` |
| `/lib`, `/usr/lib` | Compiler passes, startup objects, libraries, and data for programs | `cpp`, `c0`, `libc.a`, `crt0.o`; `ccom`, `lint1`, font tables |
| `/usr/include` | Development headers | `stdio.h`, `sys/socket.h`, `nexix/syscall.h` |
| `/etc` | Configuration and rc scripts only, never programs | `rc`, `ttys`, `passwd`, `group`, `profile`, `hostname`, `termcap`, `netstart.qemu`, an optional `netstart` |
| `/dev` | Device nodes | `console`, `ttyS0`, `null` |
| `/usr/adm` | Accounting and login records | `wtmp` |
| `/usr/man`, `/usr/pub`, `/usr/doc` | Manuals, reference tables, and licence and porting notes | `man1/ls.1`, `ascii`, `LICENSE` |
| `/root`, `/home/<user>` | Home directories | `.profile` |
| `/tmp` | Temporary files | |

**`PATH`:** root's profile and `init`'s recovery shell use
`/sbin:/bin:/usr/sbin:/usr/bin`. Users get `/bin:/usr/bin`.

## Differences from 4.4BSD

- **User commands** stay in `/bin`, as in V7, instead of being split between
  `/bin` and `/usr/bin`. `/usr/bin` holds only `wall`. This includes `ping`
  and `nc`, which BSD installs as `/sbin/ping` and `/usr/bin/nc`.
- **Compiler passes and libraries** keep their V7 home in `/lib` (`cpp`, `c0`,
  `as2`, `libc.a`, `crt0.o`), where BSD uses `/usr/lib` and `/usr/libexec`.

## The move from V7's `/etc` binaries (task 432)

| V7 path | Now |
|---|---|
| `/etc/init` | `/sbin/init` |
| `/etc/dmesg` | `/sbin/dmesg` |
| `/etc/mknod` | `/sbin/mknod` |
| `/etc/ifconfig` | `/sbin/ifconfig` |
| `/etc/route` | `/sbin/route` |
| `/etc/accton` | `/usr/sbin/accton` |
| `/etc/update` | `/usr/sbin/update` |
| `/etc/getty` | `/usr/libexec/getty` |
| `/etc/wall` | `/usr/bin/wall` |

No links remain in `/etc`.

**Upgrading an older disk:**
- The upgrade removes the old binaries. It warns about kept configuration
  that still names them, and about a root profile without `/sbin` on its
  `PATH`; edit those files afterwards. See
  [disk upgrades](disk-upgrades.md#preservation-policy).
- Until a disk is upgraded, the kernel starts its `/etc/init` and prints
  `ROOT OLD LAYOUT`.

**Manuals:** the historical V7 manuals for these programs give the new paths.
They are otherwise unchanged, as `make test-man-pages` checks.

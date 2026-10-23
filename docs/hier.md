# File system hierarchy

Nexix keeps V7's tree and follows the 4.4BSD rule for system programs (tasks
432–435):
- programs live in the binary directories;
- `/etc` holds only configuration and rc scripts.

`make test-etc-layout` enforces both rules, on the installation manifests and
on the built distribution image, and `make test-layout-upgrade` enforces them
on upgraded disks.

| Directory | Contents | Examples |
|---|---|---|
| `/bin` | Ordinary user commands (Nexix keeps V7's single `/bin` for them) | `sh`, `ls`, `cc`, `vi` |
| `/sbin` | System programs needed to boot and repair, before `/usr` | `init`, `dmesg`, `mknod`, `ifconfig`, `route`, `ping` (setuid root) |
| `/usr/bin` | User commands that BSD keeps under `/usr` | `wall`, `nc` |
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

**`PATH`:** every profile, and `init`'s recovery shell, uses
`/sbin:/bin:/usr/sbin:/usr/bin`, as in FreeBSD. Ordinary users therefore reach
`ping` by name; programs that need root still refuse them.

## Differences from 4.4BSD

The tree was compared with 4.4BSD-Lite2's `hier(7)` after tasks 432–435.
`/sbin`, `/usr/sbin`, `/usr/libexec`, `/usr/include`, `/root`, `/tmp` and the
configuration in `/etc` match. The remaining differences are mostly V7
heritage that 4.4BSD restructured. Each one that is planned to change has a
roadmap task (436–443).

### Programs

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/bin/dmesg`, a leftover link to `/sbin/dmesg` | `/sbin/dmesg` only | 436 |
| `/bin` holds about 80 commands, nearly all of userland | `/bin` holds the single-user essentials (`sh`, `cat`, `cp`, `ls`, `ln`, `rm`, `mkdir`, `mv`, `chmod`, `date`, `df`, `echo`, `ed`, `expr`, `hostname`, `kill`, `ps`, `pwd`, `rmdir`, `sleep`, `stty`, `sync`, `test`/`[`); the rest (`ar`, `as`, `awk`, `cc`, `cmp`, `grep`, `ld`, `lex`, `make`, `man`, `more`, `nroff`, `troff`, `vi`, `login`, `su`, `passwd`, `who`, `yacc`, …) is in `/usr/bin` | 440 |
| `/bin/chown`, `/bin/chgrp` | `/usr/sbin/chown`, `/usr/bin/chgrp` | 440 |
| `/bin/osh`, `arcv`, `tc`, `pcc` | none; these V7-only tools would go to `/usr/bin` | 440 |
| `/lib`: compiler passes `cpp`, `c0`, `c1`, `c2`, `fc1`, `as2` | `/usr/libexec`; 4.4BSD has no `/lib` | 439 |
| `/lib`: `libc.a`, `libm.a`, `libln.a`, `crt0.o`, `mcrt0.o`, `fcrt0.o`, `fmcrt0.o` | `/usr/lib` | 439 |
| `/usr/lib`: `ccom`, `lint1`, `lint2`, and the `lorder-native` helper | `/usr/libexec` | 439 |

### Shared data: there is no `/usr/share`

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/usr/man/man0`–`man8` | `/usr/share/man/manN` | 437 |
| `/usr/pub/ascii`, `eqnchar`, `greek` | `/usr/share/misc/` | 437 |
| `/usr/doc/vi` | `/usr/share/doc/` | 437 |
| `/usr/lib/tmac/` | `/usr/share/tmac/` | 437 |
| `/usr/lib/font/`, `/usr/lib/term/` | `/usr/share/` font and terminal tables | 437 |
| `/usr/lib/yaccpar`, `/usr/lib/lex/*form` | `/usr/share/misc/` | 437 |
| `/usr/lib/llib-lc`, `llib-lm`, `llib-port` | `/usr/libdata/lint/` | 437 |
| `/etc/termcap`, the database itself | `/usr/share/misc/termcap`, with `/etc/termcap` a link to it | 437 |

### Variable data: `/var` holds only `/var/tmp` and `/var/preserve`

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/usr/adm/wtmp` | `/var/log/wtmp` | 438 |
| `/etc/utmp`, a runtime file and the one non-configuration file in `/etc` | `/var/run/utmp` | 438 |
| The accounting file (`/usr/adm/acct` by convention) | `/var/account/acct` | 438 |
| none | `/var/log`, `/var/run`, `/var/account`, `/var/mail`, `/var/spool`, `/var/db` | 438 |

### Elsewhere

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/etc/passwd` with password hashes | `/etc/master.passwd` (hashes, root-only) with a public `/etc/passwd` and `pwd.db`/`spwd.db` | 442 |
| V7 device names (`rk0`, `rp0`, `hp0`, `tm0`, `kl0`, `dz0` and raw `r*` forms) plus native `ttyS0`/`ttyS1` | BSD names (`wd0`/`sd0`, `rwd0`, `tty00`, …) | 443 |
| No `/mnt`, no `/usr/local` | Both exist | 441 |

### Deliberate differences

These stay, for the reasons given:

- **User commands stay in `/bin`** until task 440 splits them. Until then
  `/usr/bin` holds only `wall` and `nc`.
- **`/etc/hostname`** is read by `init`, which sets the name before any login.
  4.4BSD set it from `/etc/rc`.
- **`/etc/profile`** is not in 4.4BSD; FreeBSD added it later.
- **`/etc/netstart` is run by `init`**, not by `/etc/rc` as in 4.4BSD, because
  upgrades keep an existing `rc` (see [network startup](netstart.md)).
- **`/home/<user>`** is not in 4.4BSD's `hier`; FreeBSD later used `/usr/home`,
  with `/home` a link to it.

## The moves to BSD locations (tasks 432 and 435)

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
| `/bin/ping` (task 435) | `/sbin/ping` |
| `/bin/nc` (task 435) | `/usr/bin/nc` |

No links remain at the old paths.

**Upgrading an older disk:**
- The upgrade removes the old binaries. It warns about kept configuration
  that still names them, and about a root profile without `/sbin` on its
  `PATH`; edit those files afterwards. See
  [disk upgrades](disk-upgrades.md#preservation-policy).
- Until a disk is upgraded, the kernel starts its `/etc/init` and prints
  `ROOT OLD LAYOUT`.

**Manuals:** the historical V7 manuals for these programs give the new paths.
They are otherwise unchanged, as `make test-man-pages` checks.

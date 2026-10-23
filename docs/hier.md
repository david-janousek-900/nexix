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

This comparison is checked against 4.4BSD-Lite2 sources kept in the pinned
reference set (`make test-bsd-reference`). The sources are `hier(7)`
(`reference/bsd-lite2/source/usr/src/share/man/man7/hier.7`), the `mtree` specs that define the installed tree
(`reference/bsd-lite2/source/usr/src/etc/mtree/4.4BSD.root`, `.usr` and `.var`), and the program directory
Makefiles (`bin`, `sbin`, `usr.bin`, `usr.sbin`, `libexec`). The pinned
archive's installed skeleton also shows its `etc` links.

`/sbin`, `/usr/sbin`, `/usr/libexec`, `/usr/include`, `/root`, `/home`, `/tmp`
and the configuration in `/etc` match. The remaining differences are mostly V7
heritage that 4.4BSD restructured. Each one that is planned to change has a
roadmap task (436–443). Where 4.4BSD has no counterpart, the table says so and
names the nearest fit.

### Programs

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/bin/dmesg`, a leftover link to `/sbin/dmesg` | `/sbin/dmesg` only (`sbin/Makefile`) | 436 |
| `/bin` holds about 80 commands, nearly all of userland | `/bin` holds `cat chmod cp csh date dd df echo ed expr hostname kill ln ls mkdir mv pax ps pwd rcp rm rmail rmdir sh sleep stty sync test` (`bin/Makefile`). `/usr/bin` holds the rest, including `ar`, `basename`, `cal`, `cmp`, `file`, `grep`, `ld`, `login`, `lorder`, `make`, `man`, `mesg`, `more`, `nice`, `nm`, `nohup`, `passwd`, `ranlib`, `size`, `strip`, `su`, `tail`, `touch`, `true`/`false`, `tty`, `uname`, `wc`, `who`, `write`, `yacc` and `yes` (`usr.bin/Makefile`) | 440 |
| `/bin/chown`, `/bin/chgrp` | `/usr/sbin/chown`, with `/usr/bin/chgrp` a link to it (`usr.sbin/chown/Makefile`) | 440 |
| `/bin/cc`, `as`, `awk`, `lex`, `lint`, `osh`, `arcv`, `tc`, `pcc` | Not in Lite2: its GNU and contributed tools are omitted, and `lint` is absent. `hier(7)` places programming tools and applications in `/usr/bin` | 440 |
| `/lib`: compiler passes `cpp`, `c0`, `c1`, `c2`, `fc1`, `as2` | `/usr/libexec`, for programs executed by other programs; 4.4BSD used `/usr/libexec/gcc2/`, with `/usr/bin/cpp` a wrapper script (`usr.bin/cpp/cpp.sh`). The root spec has no `/lib` | 439 |
| `/lib`: `libc.a`, `libm.a`, `libln.a`, `crt0.o`, `mcrt0.o`, `fcrt0.o`, `fmcrt0.o` | `/usr/lib`, "system C library archives" | 439 |
| `/usr/lib`: `ccom`, `lint1`, `lint2`, and the `lorder-native` helper | `/usr/libexec` | 439 |

### Shared data: there is no `/usr/share`

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/usr/man/man0`–`man8` (source pages) | `/usr/share/man`; 4.4BSD installs only formatted `catN` pages, and source `manN` directories there follow later BSDs | 437 |
| `/usr/pub/ascii`, `eqnchar` | `/usr/share/misc/ascii`, `eqnchar` | 437 |
| `/usr/pub/greek` | No counterpart (Lite2's `greek` is quiz data); `/usr/share/misc` is the nearest fit | 437 |
| `/usr/doc/vi` | `/usr/share/doc/` | 437 |
| `/usr/lib/tmac/` | `/usr/share/tmac/`; the 4.4BSD skeleton keeps `/usr/lib/tmac` as a link to it | 437 |
| `/usr/lib/font/`, `/usr/lib/term/` (the V7 troff and nroff tables) | `/usr/libdata`, for utility data files. When groff replaced these formatters, 4.4BSD moved the old tables to `/usr/old/libdata/troff_font` and `term`; Nexix still uses them, so `/usr/libdata/troff_font` and `/usr/libdata/term` | 437 |
| `/usr/lib/yaccpar`, `/usr/lib/lex/*form` | No counterpart (BSD yacc and flex compile their skeletons in); `/usr/share/misc` is the nearest fit | 437 |
| `/usr/lib/llib-lc`, `llib-lm`, `llib-port` | No counterpart (no `lint` in Lite2); `/usr/libdata/lint` follows later BSDs | 437 |
| `/etc/termcap`, the database itself | `/usr/share/misc/termcap`, with `/etc/termcap` a link to it in the skeleton | 437 |

### Variable data: `/var` holds only `/var/tmp` and `/var/preserve`

| Nexix now | 4.4BSD (var spec, `hier(7)`) | Task |
|---|---|---|
| `/usr/adm/wtmp` | `/var/log/wtmp` | 438 |
| `/etc/utmp`, a runtime file and the one non-configuration file in `/etc` | `/var/run/utmp` | 438 |
| The accounting file (`/usr/adm/acct` by convention) | `/var/account/acct` | 438 |
| none | `/var/log`, `/var/run`, `/var/account`, `/var/mail`, `/var/spool`, `/var/db` and more (`at`, `backups`, `crash`, `msgs`, `quotas`, `rwho`) | 438 |

### Elsewhere

| Nexix now | 4.4BSD | Task |
|---|---|---|
| `/etc/passwd` with password hashes | `/etc/master.passwd` (hashes, root-only), with a public `/etc/passwd` and `pwd.db`/`spwd.db` built by `pwd_mkdb` | 442 |
| V7 device names (`rk0`, `rp0`, `hp0`, `tm0`, `kl0`, `dz0` and raw `r*` forms) plus native `ttyS0`/`ttyS1` | i386 names from `etc.i386/MAKEDEV`: `wd*`/`rwd*` disks, `fd*` floppies, `com*` serial ports (`com1`, …), `wt*`/`rwt*` tape | 443 |
| No `/mnt`, no `/usr/local` | `/mnt` (root spec); `/usr/local` with `bin`, `sbin`, `lib`, `libexec`, `libdata` and `man` (usr spec), and `include` (`hier(7)`) | 441 |

### Deliberate differences

These stay, for the reasons given:

- **User commands stay in `/bin`** until task 440 splits them. Until then
  `/usr/bin` holds only `wall` and `nc`.
- **`/etc/hostname`** is read by `init`, which sets the name before any login.
  4.4BSD set it from `/etc/rc`.
- **`/etc/profile`** is not in 4.4BSD; FreeBSD added it later.
- **`/etc/netstart`** is an executable script run by `/etc/rc` in 4.4BSD. In
  Nexix the sample is not executable and `init` runs the script with `sh`,
  because upgrades keep an existing `rc` (see [network startup](netstart.md)).
- **Compatibility links:** 4.4BSD itself kept a few (`/etc/termcap`,
  `/etc/rmt`, `/usr/lib/tmac`). Nexix leaves none at relocated program paths
  (tasks 432–435); task 437 decides whether data links such as
  `/etc/termcap` are kept.

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

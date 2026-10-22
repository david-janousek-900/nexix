# Nexix v0.1.0 source audit

OpenBSD-style audit of the code at revision `f20b407` (branch `v0.1.0-issues`): each subsystem was read line by line, looking for memory-safety bugs, integer overflows, unchecked return values, /tmp races, privilege mistakes, reference-count leaks and error paths that corrupt state. Nothing was built or run.

- Reviewers dropped every finding they could not confirm by reading the code, plus intentional V7 behaviour.
- The 124 existing GitHub issues were excluded, as were duplicates between reviewers.
- I spot-checked the high-severity items myself (#2, #4, #7, #9, #16).
- **Total: 60 findings** (8 high, 31 medium, 21 low).
- Filed as GitHub issues #125–#184 (finding N is issue #(124+N)).

Background that several findings depend on:
- Setuid programs: `passwd su newgrp mkdir mv rmdir expreserve`.
- `/tmp` is mode 0777 without the sticky bit and sits on the same filesystem as `/etc`, so anyone can hard-link a victim's file into it. There are no symlinks.
- A pending `alarm()` survives `exec` (`p_clktim` is cleared only at exit, `kernel/v7/process.c:488`). A user can therefore kill a setuid program at a time of their choosing, even though `kill()` itself is refused.

## High

| # | Area | Location | Finding | Suggested fix |
|---|---|---|---|---|
| 1 | passwd (setuid) | `user/v7/cmd/passwd.c:170`, `user/v7/password-file.c:392-403` | SIGALRM is not ignored. An inherited alarm can kill passwd between `unlink("/etc/passwd")` and `link(candidate,…)`, leaving **no /etc/passwd**, so nobody can log in. An earlier kill leaves `/etc/ptmp` behind, which blocks every later passwd. | `alarm(0)` and block all catchable signals; publish with an atomic `rename()`. |
| 2 | sh | `user/v7/sh/io.c:99-102`, `main.c:20,170` | The here-document temp file `/tmp/sh<pid><n>` is opened with `creat()`, not O_EXCL. A hard link planted there makes root's shell truncate and overwrite any file on the root filesystem. | `O_CREAT\|O_EXCL`, mode 0600; retry on EEXIST. |
| 3 | cc | `user/v7/cmd/cc.c:197-221` | Only `ctm0X` is probed (with a racy `access()`). `ctm1X`–`ctm5X` are opened for writing unchecked, so planted links clobber files when root compiles. | Private `mkdtemp` directory for all intermediate files. |
| 4 | troff/nroff | `user/troff/n1.c:337-350` | Temp file from `mktemp` + `creat()` with no O_EXCL: the victim's file is clobbered and macro text can be injected. The `open()` result is unchecked. | `mkstemp`; check the fd. |
| 5 | vi | `user/vi/ex_cmdsub.c:678-750` | The pattern field of a tags file is run as an **arbitrary ex command** (`!sh …`), and the filename is globbed through the shell. An untrusted tags file means command execution. | Accept only `/pat/`, `?pat?` or a number; treat the filename literally. |
| 6 | cp | `user/v7/cmd/cp.c:10,47-59` | `dir/basename` is built in `iobuf[512]` with unbounded loops, overflowing into .bss. | Use a PATH_MAX buffer and a length check. |
| 7 | mv (setuid) | `user/v7/cmd/mv.c:160-217` | `mvdir` checks paths with `access()` and then calls `link`/`unlink` by name as root. A race lets a user wire directories into places they can't write, or create loops. | Drop privileges and use a kernel rename, or work from fds. |
| 8 | lex | `user/lex/sub1.c:256-262`, `parser.y:599,615` | Octal escapes have no digit limit (`[\7777]`), so input can write outside `symbol[NCH]`. | Allow at most 3 digits, mask with 0377, range-check before indexing. |

## Medium

| # | Area | Location | Finding | Suggested fix |
|---|---|---|---|---|
| 9 | mkdir/rmdir (setuid) | `mkdir.c:16-20,50-71`; `rmdir.c:169-178` | An alarm, or INTR for rmdir (which ignores nothing), can stop them midway, leaving directories without `.`/`..` and wrong link counts. | `alarm(0)`; block signals around the sequence. |
| 10 | kernel exec/ptrace | `kernel/v7/files.c:245`, `exec.c:74`, `ptrace.c:60` | A traced process can exec a binary that is execute-only for it, and the tracer then dumps the text with `PT_READ_I`. V7's `getxfile` read check is missing. | When STRC is set, also require read permission. |
| 11 | kernel fs | `kernel/v7/files.c:101-102` | An inode that is still named in a directory but has on-disk `nlink==0` is freed on last close, so the name later points at a reused inode. On a read-only volume the release fails with EROFS and stays pending, so umount/sync are busy forever. | Reject nlink 0 on lookup; never release on a read-only volume. |
| 12 | libc time | `libc/time.c:95,131,155-164`; `user/v7/cmd/ls.c:207` | `ctime` returns NULL for years outside 1–9999. Setting a file's `mtime` with `utime()` then makes `ls -l` crash with SIGSEGV for anyone listing that directory. | Return a placeholder string; guard ls. |
| 13 | rm | `user/v7/cmd/rm.c:90,145` | The raw `wait` status (256) is added to `errcode`, so `exit()` truncates it to 0: a failed `rm -r` reports success. | Return `status!=0`; `exit(errcode?1:0)`. |
| 14 | sh | `user/v7/sh/io.c:129`, `macro.c` flush | Here-document writes are unchecked, so a full disk silently feeds commands a truncated here-document. | Check writes; fail the command. |
| 15 | cat | `user/v7/cmd/cat.c:45-62` | Always exits 0; write and flush errors are ignored, so output is silently truncated. | Track errors; check `ferror`/`fflush`. |
| 16 | cp | `user/v7/cmd/cp.c:44,73-89` | `close(fnew)` and `fstat` are unchecked; a partial target is reported as success. | Check both. |
| 17 | lex | `user/lex/sub1.c:10-11` | `getl()` copies a line of any length into `buf[520]`. | Pass the size and bound the copy. |
| 18 | lex | `user/lex/parser.y:499-654` | Writes to `token[TOKENSIZE]` are unbounded in 4 loops; the string loop is off by one. | Add an `i<TOKENSIZE-1` guard. |
| 19 | lex | `user/lex/parser.y:44-54,371-377` | `dchar` and `schar` are copied into before the space check runs. | Check the remaining space first. |
| 20 | eqn/neqn | `user/eqn/lex.c:128-136` (+neqn) | Each backslash stores 2 bytes but counts 1, overflowing `token[400]`. | Count every byte. |
| 21 | eqn/neqn | `user/eqn/e.y:48-51,126`, `glob.c:4` (+neqn) | `lp[80]` pile/matrix stack has no bound. | Check `ct<80`. |
| 22 | troff | `user/troff/n1.c:205-208` | The `-m` argument overflows `nextf[64]`. | Bounded copy. |
| 23 | troff | `user/troff/n1.c:224-229` | The `-T` argument overflows `termtab[64]`. | Bounded copy. |
| 24 | troff | `user/troff/n9.c:101-110` | `\o` with 10 or more characters writes `o[10]`/`w[10]` (stack). | Arrays `[NOV+1]`. |
| 25 | troff | `user/troff/n8.c:198,263` | `digram`/`suffix` overflow `hyptr[NHYP]`. | Bound `hyp`. |
| 26 | C compiler c2 | `user/c/c21.c:86-94,187-196` | Operands truncated to 127 characters compare as equal, so a load is removed and the **wrong code is generated** silently. | Don't cache truncated operands. |
| 27 | vi | `user/vi/ex_cmdsub.c:1204,1236-1266` | `:map`/`:ab` overflow `lhs[100]`/`rhs[100]` on the stack. | Bound both loops. |
| 28 | vi | `user/vi/ex_temp.c:486-488` | `rfname` is sized from the current `dir` option but filled from the old `tfname`: heap overflow after `:set dir=/`. | Allocate `strlen(tfname)+1`. |
| 29 | vi | `user/vi/ex_cmdsub.c:471-475` | `pragged` overflows `linebuf` with `strcat`. | Length check. |
| 30 | exrecover | `user/vi/exrecover.c:357,415-432` | `ecount` is never capped at 50. Another user can hard-link the victim's Ex files 50+ times, overflowing the stack in the victim's `vi -r`. | Cap at NENTRY; skip files with `nlink>1`. |
| 31 | exrecover | `user/vi/exrecover.c:319`, install manifest | Not setuid, so `unlink` in `/var/preserve` always fails silently: stale copies pile up and override newer saves on `vi -r`. | Fix the install mode/ownership model; report the unlink failure. |
| 32 | expreserve (setuid) | `user/vi/expreserve.c:461-483` | In stdin mode, any amount of data is copied as root into `/var/preserve` (root bypasses the reserved blocks), filling the root filesystem. | Cap the copy at the size from the header. |
| 33 | tty driver | `kernel/v7/tty.c:132-136`, `serial.c:48-52` | Every open of a DH line with SECONDARY set resets the speed and drains output **with IRQs off**: up to about 136 s system freeze. | Only on the first open; sleep until the line is idle. |
| 34 | ls | `user/v7/cmd/ls.c:18,305,405` | A 14-character name is not NUL-terminated (memory from `malloc`), so `strcmp` reads beyond it. | Terminate it, or use `calloc`. |
| 35 | init | `user/v7/cmd/init.c:249,338,140-162` | `merge()` runs inside the SIGINT handler and can re-enter the main loop: duplicate gettys, lost pids, wrong utmp entries. | The handler sets a flag; merge from the loop. |
| 36 | su (setuid) | `user/v7/cmd/su.c:41-49` | The whole caller environment (`PATH` starting with `:`, `IFS`) is passed to the root shell. | Clean environment. |
| 37 | write | `user/v7/cmd/write.c:278-313` | Raw escape sequences are written to the recipient's terminal (screen spoofing); stdio is used from the signal handler. | Filter control characters; use a flag in the handler. |
| 38 | getgrent | `libc/group.c:42-49` | A group with 100 or more members is skipped entirely, so newgrp can't find it and ls shows a gid. | Truncate the member list, as V7 did. |
| 39 | crypt | `libc/crypt.c:377-408` | A 1-character salt produces a hash that can never be verified, locking the account. | Normalise or reject the salt. |

## Low

| # | Area | Location | Finding | Suggested fix |
|---|---|---|---|---|
| 40 | kernel | `kernel/arch/x86_64/paging.c:111-151` | SMEP/SMAP are not enabled, though uaccess never touches user virtual addresses, so they would cost nothing. | Set CR4 bits 20/21 when CPUID reports them. |
| 41 | kernel fs | `kernel/v7/fs.c:183-238,492` | Free list and indirect blocks are not checked for duplicates, so a corrupt image can double-allocate a block. | Allocation bitmap at mount. |
| 42 | kernel fs | `kernel/v7/fs.c:76`, `files.c:93,394,580` | 24-bit on-disk device numbers are truncated to 16 bits inconsistently between classification and open. | EIO when `addr[0]>0xFFFF`. |
| 43 | rmdir (setuid) | `user/v7/cmd/rmdir.c:114-147` | Probes as root before any access check: reveals whether paths in unreadable directories exist and are empty. | Check with the real uid first. |
| 44 | wall | `user/v7/cmd/wall.c:5,24,43-67` | Reads only 50 utmp slots and never reaps children (zombies). | Read until EOF; wait. |
| 45 | cc | `user/v7/cmd/cc.c:48-51,172-187` | A long `-B` prefix overflows the `char[20]` pass names. | `snprintf` into PATH_MAX buffers. |
| 46 | printf | `libc/stdio.c:311` | `%.Ns` reads one byte past the precision. | Check the bound first. |
| 47 | scanf | `libc/scan.c:97-99` | Format bytes ≥0x80 are sign-extended, so literal matches fail. | `(unsigned char)`. |
| 48 | freopen | `libc/stdio.c:54-55` | Closes the stream before validating the mode, so fd 0/1 can be lost. | Validate first. |
| 49 | ed | `user/v7/cmd/ed.c:527,347` | `close()` after `w` is unchecked, so the buffer is marked saved. | Check it; keep `fchange`. |
| 50 | getty | `user/v7/cmd/getty.c:207-239` | A name longer than 16 characters loses a character, CRMOD is left unset, and the rest of the line goes to the password prompt. | Discard input up to the terminator. |
| 51 | ar | `user/v7/cmd/ar.c:70-102,153-190` | `ar r` drops, or duplicates, members whose basenames are equal; exit 0. | Reject or report unused operands. |
| 52 | pwd | `user/v7/cmd/pwd.c:290-307` | A path longer than 512 bytes prints a truncated wrong path; exit 0. | Error and exit 1. |
| 53 | wc | `user/v7/cmd/wc.c:21-73` | `wc -` prints no counts. | Treat an empty option set as the default. |
| 54 | eqn/neqn | `user/eqn/lex.c:68-76` | 1-byte overflow of `token[400]`. | Check `sp>=SSIZE-2`. |
| 55 | troff | `user/troff/n1.c:202` | `-r` with no name writes `vlist[-1]`. | Check `findr` for -1. |
| 56 | troff | `user/troff/n3.c:332`, `n5.c:426` | Temp file I/O unchecked, so output is silently corrupted when space runs out. | Check; exit nonzero. |
| 57 | expreserve | `user/vi/expreserve.c:518-536` | `mknext` loops forever once the names are exhausted. | Return an error. |
| 58 | vi | `user/vi/ex_set.c:193-224` | `:set` string option off by one (`optname`, `direct[]`). | `ONMSZ-1`. |
| 59 | vi | `user/vi/ex_io.c:1067-1071` | Modeline `strncpy` leaves `cmdbuf` unterminated. | Terminate explicitly. |
| 60 | build tools | `tools/mk_v7fs.py:190-195`, `seed_disk.py:125-129` | Images are always 0600, losing the existing mode (upgrade_disk does keep it). | `fchmod` before replace. |

## Areas reviewed and found clean
- **Kernel exec, ELF and memory:** `elf.c`, `exec.c`, `paging.c`, `memory.c`, `phys.c`, `core.c`, `profile.c`, `kinfo.c`.
- **Kernel I/O and drivers:** the packet/mpx/modem/tape drivers, apart from issues already filed.
- **Toolchain:** yacc, the `toolchain/` archive and object code, `ld` relocations.
- **Host tools:** `v7_archive.py` (path traversal is checked), `upgrade_disk.py`.
- **Login and session:** login buffers, `session.c`.
- **Editors:** the editor's own temp files in vi (O_EXCL|O_NOFOLLOW), and ed's temp files.

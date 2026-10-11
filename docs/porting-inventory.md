# V7 porting inventory (step 21)

Nexix 0.1.0 targets multi-user boot and the full native V7 userland. The first
20 steps provide a bootable single-user system with persistent IDE storage.
This inventory accounts for the remaining work; it does not implement ports.

The authoritative [manifest](v7-userland-manifest.json) owns all **1,121 files**
under `reference/v7/usr/src`: **174 components** and **35 external/platform
requirements**. Components include 120 top-level command sources, 34 command
directories, eight games, eight library directories, three build scripts and
one top-level support header. Shared sources can have multiple owners.

Only **init, sh, echo, cat, ls, pwd, cp, ln and rm** are installed native ports.
Native libc is partial. Other components are unported or explicitly classified
as prototypes; historical hardware exceptions remain pending assessment.

## Manifest contract

- Each entry has an identifier, kind, explicit reference files, outputs and
  install locations, priority, status, dependencies, blockers and acceptance checks.
- Dependencies distinguish build, runtime and optional runtime requirements.
  Confirmed edges cite local evidence; inferred edges are labeled. Transitive
  syscall/libc/ABI audits are explicitly unresolved. Cycles are permitted for
  bootstrap tools and generated support assets.
- Includes are recorded with source evidence; an include does not prove that
  Nexix implements every declaration or ABI used by the program.
- Output roles distinguish installed programs, libraries, optional targets,
  private helpers, build scripts, generated data and source-only material.
  Install paths describe reference recipes; null means no established location.
- Historical source archives and prebuilt artifacts are listed separately.
  Their ownership does not make old objects native or complete archive-member audits.
- Platform/data requirements are supplied, partial, generated, missing or
  pending assessment. Generated means a recipe exists, not that Nexix ships it.
- Preserve V7 atime/mtime/ctime semantics and disk widths. There is no separate
  creation-time field and no planned timestamp-format extension.

## Implementation priorities

These are implementation order, not release exclusions. RTC, timestamps, timezone,
kernel/libc prerequisites and multi-user terminals are steps 22–27. The remaining
program ports belong to step 28. Shared libc support is developed as needed.

| Priority | Group | Top-level commands |
|---|---|---|
| 0 | Foundational libc | See groups below |
| 1 | Time and metadata | `date`, `touch` |
| 2 | Multi-user startup | `getty`, `init`, `login`, `mesg`, `newgrp`, `passwd`, `stty`, `su`, `tty`, `wall`, `who`, `write` |
| 3 | Essential filesystem and shell tools | `basename`, `cal`, `cat`, `chgrp`, `chmod`, `chown`, `cp`, `echo`, `expr`, `file`, `kill`, `ln`, `ls`, `mkdir`, `mv`, `nice`, `osh`, `pwd`, `rm`, `rmdir`, `sleep`, `sync`, `test`, `yes` |
| 4 | Text processing and editing | `cmp`, `col`, `comm`, `diff`, `diff3`, `diffh`, `ed`, `egrep`, `fgrep`, `find`, `grep`, `join`, `look`, `od`, `pr`, `rev`, `sort`, `sp`, `split`, `tabs`, `tail`, `tee`, `tr`, `tsort`, `uniq`, `wc` |
| 5 | Build generators and helpers | `cb` |
| 6 | Native C toolchain | `ar`, `arcv`, `cc`, `ld`, `nm`, `ranlib`, `size`, `strip` |
| 7 | Mathematics and language runtimes | `bc`, `factor`, `primes`, `random`, `spline`, `units` |
| 8 | Storage administration and archives | `clri`, `dcheck`, `dd`, `df`, `du`, `dump`, `dumpdir`, `icheck`, `mkfs`, `mknod`, `mount`, `ncheck`, `quot`, `restor`, `sum`, `umount`, `update` |
| 9 | Jobs, mail and accounting | `ac`, `accton`, `at`, `atrun`, `calendar`, `cron`, `crypt`, `mail`, `makekey`, `sa` |
| 10 | Diagnostics and advanced development | `dmesg`, `iostat`, `prof`, `ps`, `pstat`, `time` |
| 11 | Documents, references and learning | `checkeq`, `deroff`, `ptx` |
| 12 | Plotting and historical output | `graph`, `tc`, `tk`, `vpr` |
| 13 | Serial communications | `cu` |
| 14 | Games | See groups below |
| 15 | Historical standalone support | See groups below |

## Multi-output source groups

| Source group | Outputs | Priority |
|---|---|---|
| `adb` | `adb` | 10 |
| `as` | `as`, `as2` | 6 |
| `awk` | `awk`, `proc` | 4 |
| `bas` | `bas` | 7 |
| `c` | `c0`, `c1`, `c2`, `fc1`, `cvopt` | 6 |
| `cpp` | `cpp` | 5 |
| `dc` | `dc` | 7 |
| `eqn` | `eqn` | 11 |
| `f77` | `f77`, `f77pass1` | 7 |
| `learn` | `learn`, `tee`, `lcount` | 11 |
| `lex` | `lex`, `smallex`, `libln.a` | 5 |
| `lint` | `lint`, `lint1`, `lint2` | 10 |
| `m4` | `m4` | 5 |
| `make` | `make` | 5 |
| `mip` | `comp`, `fort` | 6 |
| `neqn` | `neqn` | 11 |
| `pcc` | `ccom`, `fort` | 6 |
| `plot` | `tek`, `t300`, `t300s`, `t450`, `vplot` | 12 |
| `prep` | `prep` | 11 |
| `ratfor` | `ratfor` | 7 |
| `refer` | `refer`, `mkey`, `inv`, `hunt`, `deliv`, `refpart`, `whatabout`, `flagger`, `kaiser`, `thash` | 11 |
| `roff` | `roff` | 11 |
| `sed` | `sed` | 4 |
| `sh` | `sh` | 3 |
| `spell` | `spell`, `spellin`, `spellout` | 11 |
| `standalone` | `cat`, `restor`, `icheck`, `mkfs`, `boot`, `mtboot`, `maketape`, `libsa.a`, `srt0.o` | 15 |
| `struct` | `structure`, `beautify` | 7 |
| `tar` | `tar`, `v6tar` | 8 |
| `tbl` | `tbl` | 11 |
| `tp` | `tp` | 8 |
| `troff` | `nroff`, `troff`, `mkfont`, `chars` | 11 |
| `uucp` | `uucp`, `uux`, `uulog`, `uuxqt`, `uucico`, `uuclean`, `uurecover` | 13 |
| `xsend` | `enroll`, `xsend`, `xget` | 9 |
| `yacc` | `yacc` | 5 |

Auxiliary executables without install recipes remain helpers. `mip` is shared
compiler infrastructure with prototype targets, not an installed command.
The `bas` makefile explicitly states that obsolete library interfaces prevent
its supplied build. Standalone tools reuse top-level sources where their build
script specifies them. Their prebuilt PDP-11 objects need native replacements.

## Libraries and games

| Library group | Runtime outputs |
|---|---|
| `libF77` | `libF77.a` |
| `libI77` | `libI77.a` |
| `libc` | `libc.a`, `crt0.o`, `mcrt0.o`, `fcrt0.o`, `fmcrt0.o` |
| `libdbm` | `libdbm.a` |
| `libfpsim` | `fpsim.o` |
| `libm` | `libm.a` |
| `libmp` | `libmp.a` |
| `libplot` | `libplot.a`, `libt4014.a`, `libt300.a`, `libt300s.a`, `libt450.a`, `vt0 backend source` |

`libln.a` belongs to the lex group; `libsa.a` and `srt0.o` belong to standalone.
`libfpsim` builds `fpsim.o` for insertion into libc, not a separate libfpsim archive.
`libdbm` historically renames an object to libdbm.a; native archive packaging
needs adaptation. The vt0 backend is source-only in the default libplot recipe.

Games: **arithmetic, backgammon, chess, fish, fortune, hangman, quiz, wump**.
All are priority 14. Chess has substantial PDP-11 assembly; fortune, hangman
and quiz also require data absent from this snapshot.

## Missing distribution commands and manuals

**`man` is required for Nexix 0.1.0, priority 11, step 28.** Its implementation
and the `/usr/man` page tree are absent from this imported reference snapshot.
They are explicitly tracked as the manifest requirements `man-command` and
`manual-pages`, not represented as existing source files or implemented ports.
Restore the original V7 implementation and pages, verify its exact dependencies
and command syntax, and package the required nroff/troff macros and terminal
tables. Acceptance includes finding/rendering representative manual sections,
readable serial output and reference-compatible missing-page behavior.

The original V7 [filesystem hierarchy manual](https://www.tuhs.org/cgi-bin/utree.pl?file=V7%2Fusr%2Fman%2Fman7%2Fhier.7)
documents the manual tree and formatter macros. Full-distribution comparison is
still needed to identify further commands/scripts/data missing from this import.
The 174-component count describes supplied source components, not every program
in the original installed V7 distribution.

## Dependencies and missing inputs

Important source-confirmed chains include bc → dc and optional lib.b;
crypt → makekey; diff → diffh; graph → libplot/libm; awk → yacc/lex/libm;
lex → yacc and scanner skeletons; PCC/lint → shared mip sources;
xsend/xget/enroll → libmp; beautify → libln; spell hashes → spellin and dictionaries.
Host tools bootstrap generator cycles; native compiler retargeting is still required.

Kernel adaptations are required for independent TTYs, authenticated credentials,
accounting, tracing/profiling, raw device files and mount administration.
Programs such as ps/pstat/iostat inspect historical kernel layouts; compiling
their sources alone does not provide a working native equivalent.

| Requirement | State | Evidence / expected location |
|---|---|---|
| `native-build-tools` | supplied | Host GCC/binutils bootstrap native Nexix ELF builds; retained native tools remain unported. |
| `headers` | partial | `/usr/include` |
| `kernel-abi` | partial | File/process/TTY syscalls exist; mount administration, accounting, tracing, profiling and complete device interfaces need auditing. |
| `floating-point` | missing | Native user FP state isolation and full floating-point libc support are not implemented. |
| `wall-clock` | partial | Clock advances from epoch zero; RTC initialization and ordinary inode timestamp updates are pending steps 22–23. |
| `timezone` | missing | UTC-only ctime currently; configurable TZ and local-time/DST conversion are step 24. |
| `accounts` | partial | `/etc/passwd`, `/etc/group` |
| `multi-tty` | partial | Current UART and line discipline provide one shared COM1 console; independent COM2 and login terminals are pending. |
| `accounting` | partial | `/etc/utmp`, `/usr/adm/wtmp` |
| `kernel-information` | missing | Historical kernel-memory/symbol consumers need a native information interface. |
| `tracing-profiling` | missing | Native ptrace/profiling and executable register/symbol interpretation require implementation. |
| `raw-storage` | partial | IDE block operations exist internally; raw device files and mount-management interfaces are pending. |
| `historical-devices` | pending_assessment | PDP-11 disks/tapes, plotters and printer protocols require native equivalents or individually documented exceptions. |
| `elf-toolchain` | missing | Retarget retained compiler/assembler/linker and archive/symbol utilities to native x86_64 ELF. |
| `spool-locks` | missing | Provision mail/job/UUCP spool directories, ownership, locking and daemon configuration. |
| `yacc-skeleton` | missing | `/usr/lib/yaccpar` |
| `lex-skeletons` | missing | `/usr/lib/lex/ncform`, `/usr/lib/lex/nrform`, `/usr/lib/lex/ebcform` |
| `lint-descriptions` | missing | `/usr/lib/llib-lc`, `/usr/lib/llib-port` |
| `word-dictionary` | missing | `/usr/dict/words` |
| `spell-lists` | supplied | American/British/local word additions and stop list are supplied in the reference spell directory. |
| `spell-hashes` | generated | `/usr/dict/hlista`, `/usr/dict/hlistb`, `/usr/dict/hstop` |
| `learn-lessons` | missing | `/usr/lib/learn` |
| `learn-workdirs` | generated | `/usr/lib/learn/play`, `/usr/lib/learn/log` |
| `formatter-tables` | generated | `/usr/lib/font`, `/usr/lib/term` |
| `formatter-macros` | missing | `/usr/lib/tmac` |
| `bibliography-data` | missing | `/usr/dict/papers/Ind`, `/usr/lib/eign`, `/usr/dict/lookall/All` |
| `units-data` | missing | `/usr/lib/units` |
| `bc-math` | missing | `/usr/lib/lib.b` |
| `fortune-data` | missing | `/usr/games/lib/fortunes` |
| `quiz-data` | missing | `/usr/games/quiz.k/index` |
| `v7-source-archives` | supplied | libplot backends contain source in historical .c.a archives; bootstrap extraction must support the historical archive format. |
| `v6-compatibility` | pending_assessment | V6 syscall wrappers are supplied for historical compatibility; determine whether native variants are appropriate. |
| `man-command` | missing | Restore original implementation; verify install path and dependencies |
| `manual-pages` | missing | `/usr/man`; formatter macros and terminal tables |
| `pdp11-assembly` | pending_assessment | Rewrite or retarget PDP-11 assembly; do not treat successful host compilation as native compatibility. |

This is a source snapshot, not a complete installed V7 root. It lacks `man`,
`/usr/man`, and has no
`usr/include`, yacc/lex installed skeletons, lint library descriptions, base word
dictionary, lesson archives, formatter macro packages, units database, optional
bc math library, fortune database or quiz dataset. Font/terminal-table sources
and spell word additions are supplied; their installed/generated forms need work.

## Coverage validation

```sh
make TOOLCHAIN_PREFIX= test-inventory
```

The read-only checker validates identifiers, source/evidence paths, dependency
references, classifications, acceptance checks, all reference-file ownership
and top-level components. It compares claimed implemented outputs with the
Makefile and root-image packaging declarations. Negative tests ensure missing
sources, concealed components, unsupported claims and missing external requirements
fail validation. New reference files must be explicitly classified.

A passing check means imported-source inventory coverage and consistency. It does not prove
coverage of the complete installed V7 distribution,
complete dependency closure, source compatibility or functional port acceptance.

## Kernel foundation

Kernel reference sources remain under usr/sys/{sys,dev,h,conf}. Nexix adapts
V7 algorithms and behavior around a native x86_64 ABI; PDP-11 startup,
segmentation, assembly and swapping assumptions are replaced. Current foundations
include protected address spaces, processes/signals, native ELF execution, file
descriptors/pipes, one serial TTY, V7 disk encoding and synchronous IDE persistence.
See [architecture](architecture.md), [userland](userland.md) and
[syscall ABI](syscall-abi.md) for implemented behavior and current limits.

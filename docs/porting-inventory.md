# V7 porting inventory (task 21)

Nexix 0.1.0 targets the complete V7 source distribution through native kernel,
userland, toolchain and historical-platform equivalents, with multi-user boot. The first
20 tasks provide a bootable single-user system with persistent IDE storage.
This inventory accounts for the remaining work; it does not implement ports.

The authoritative [manifest](v7-userland-manifest.json) accounts for all
**2,144 distribution files**, grouped into **217 components**, plus **35 platform
or supporting-input requirements**. The preserved userland source subset still
contains 1,121 files and its original 174 component records. The full audit adds
installed scripts, additional games, the root diagnostic source, kernel/boot
references, and supporting-data groups. All archive contents are preserved.

**init, sh, echo, cat, ls, pwd, cp, ln, rm, date and touch** are installed native ports.
Native libc is partial. The archive's **213 installed program paths** are listed
separately; scripts and old executables in reference/v7 are not installed Nexix
ports. **13 binary-only program components** are blocked pending source recovery
or an explicitly approved alternative. No historical hardware exception is
silently excluded from the release objective.

The [distribution index](../reference/distribution.json) pins each file's bytes
and original metadata to the recorded archive. See [source provenance](../reference/README.md).

## Manifest contract

- Schema version 2 covers reference/v7, with usr/src separately identified.
- The required roadmap_coverage path selects [roadmap-coverage.json](roadmap-coverage.json).
  All components and requirements map to explicit README task IDs; see the
  [coverage/status contract](roadmap-coverage.md). Task mappings describe planned
  responsibility and do not change native implementation status.
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
  Output install paths describe reference recipes; distribution_outputs and the
  installed-program index record actual archive locations. Null means unresolved.
- The `sources` field owns reference inputs, including data and historical binaries.
  `source_files` identifies implementation source/scripts, and `source_availability`
  distinguishes supplied source, binary-only programs and non-program assets.
- Historical source archives, lesson-data archives and prebuilt artifacts are
  listed separately. Archive-member compatibility audits remain explicit future work.
- Installed program paths are mapped to owning components, including script
  wrappers and private helpers. Exact distribution locations supplement recipe paths.
- Platform/data requirements are supplied, partial, generated, missing or
  pending assessment. Generated means a recipe exists, not that Nexix ships it.
- Preserve V7 atime/mtime/ctime semantics and disk widths. There is no separate
  creation-time field and no planned timestamp-format extension.

## Implementation priorities

Priorities guide implementation order, not release exclusions. The expanded
361-task roadmap is in [README.md](../README.md#roadmap). RTC/timestamps/TZ are
tasks 22–24; kernel/libc work spans 25–82, terminals 89–93, and multi-user
integration 94–108. Individual component ports have their own IDs in the
[coverage map](roadmap-coverage.json); shared support is developed as needed.

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

Games with source: **arithmetic, backgammon, chess, fish, fortune, hangman, quiz, wump**.
All are priority 14. Chess has substantial PDP-11 assembly. The full archive
supplies the data for fortune, hangman and quiz; native format compatibility
still needs validation. Additional scripts and binary-only games are listed below.

## Restored installed scripts and additional programs

`man` is supplied as **reference/v7/bin/man**, an original Bourne-shell script.
The archive also supplies all 307 manual files across sections 0–8. Its default
path invokes nroff with the supplied tmac.an macros; optional modes use eqn,
neqn, troff and tc. The script and its manual differ on cached-page behavior;
retain and document the supplied implementation rather than inventing a new one.
Porting remains priority 11: task 287 for the script and task 286 for manual pages. Test section selection, missing titles,
`man -w`, readable serial output and available optional output modes.

Restored /bin scripts and drivers:

`1`, `calendar`, `diff3`, `false`, `lint`, `lookbib`, `lorder`, `man`, `nohup`, `pcc`, `plot`, `spell`, `struct`, `true`.

`words` and `ching` are additional game scripts. Script wrappers must be tested
with their underlying programs: restoring only the compiled helpers does not
restore complete user-visible commands. The root `bs.c`/`bs` diagnostic is also
accounted for. Documentation-driving scripts and bibliography index helpers
remain owned by their supporting-asset groups.

Binary-only components whose implementation source is absent from this archive:

`banner`, `bcd`, `bj`, `checkers`, `cno`, `cubic`, `maze`, `moo`, `phx`, `ppt`, `reversi`, `ttt`, `words1`.

The ching cno/phx executables are private game helpers. Historical binaries are
preserved for provenance and comparison; source recovery or an explicitly
approved alternative is required for native release acceptance. Importing every
archive file resolves distribution coverage, not these source-availability gaps.

## Dependencies and missing inputs

Important source-confirmed chains include bc → dc and optional lib.b;
crypt → makekey; diff → diffh; graph → libplot/libm; awk → yacc/lex/libm;
lex → yacc and scanner skeletons; PCC/lint → shared mip sources;
xsend/xget/enroll → libmp; beautify → libln; spell hashes → spellin and dictionaries.
Host tools bootstrap generator cycles; native compiler retargeting is still required.

Kernel adaptations are required for independent TTYs, authenticated credentials,
remaining device interfaces. Profiling sampling/startup/monitor records exist.
Kernel exit accounting and parent/child tracing with native register access,
debugger writes, single-step and profil sampling are implemented.
Block/raw files and the multi-volume mount/unmount namespace are implemented.
Programs such as ps/pstat/iostat inspect historical kernel layouts; compiling
their sources alone does not provide a working native equivalent.

| Requirement | State | Supplied inputs / remaining work |
|---|---|---|
| `native-build-tools` | supplied | Host GCC/binutils bootstrap native Nexix ELF builds; retained native tools remain unported. |
| `headers` | partial | Original inputs are supplied in the full archive. Adapt declarations and layouts to the native ABI; complete Nexix libc coverage remains partial. |
| `kernel-abi` | partial | File/process/TTY syscalls, device-qualified inodes, mount/umount, chroot, process accounting and parent/child tracing with debugger writes/single-step exist. Native profil sampling is implemented; remaining device interfaces are pending. |
| `floating-point` | partial | Native FP process/signal state, binary64 helpers, decimal formatting and retained libm C sources are implemented; complete libc coverage and installed math headers/archive remain pending. |
| `wall-clock` | supplied | RTC initializes the UTC system clock; inode timestamps and sync superblock time use that clock and persist in the existing V7 disk encoding. |
| `timezone` | supplied | Native time conversion and configurable TZ support fixed offsets, one-hour DST, explicit M rules and historical V7 US defaults; see time.md for the supported grammar. |
| `accounts` | partial | `/etc/passwd`, `/etc/group` |
| `multi-tty` | partial | Current UART and line discipline provide one shared COM1 console; independent COM2 and login terminals are pending. |
| `accounting` | partial | Kernel exit accounting, native records and privileged acct control exist; utmp/wtmp are packaged, login integration and accton/sa command ports remain pending. |
| `kernel-information` | missing | Historical kernel-memory/symbol consumers need a native information interface. |
| `tracing-profiling` | partial | Native tracing, debugger writes/single-step, profil sampling and monitor/mcount with profiling startup/records exist. Symbol consumers and the adb/prof command ports remain pending. |
| `raw-storage` | partial | Block/raw device files and multi-volume mount/umount routing exist; mounted-device writes are guarded, storage-administration command ports remain pending. |
| `historical-devices` | pending_assessment | PDP-11 disks/tapes, plotters and printer protocols require native equivalents or individually documented exceptions. |
| `elf-toolchain` | missing | Retarget retained compiler/assembler/linker and archive/symbol utilities to native x86_64 ELF. |
| `spool-locks` | missing | Historical spool and UUCP configuration fixtures are supplied; native services, ownership and locking remain pending. |
| `yacc-skeleton` | supplied | `/usr/lib/yaccpar` |
| `lex-skeletons` | partial | C scanner skeleton ncform is supplied. Optional Ratfor nrform and EBCDIC ebcform are referenced by source but absent from the full archive. |
| `lint-descriptions` | supplied | `/usr/lib/llib-lc`, `/usr/lib/llib-port` |
| `word-dictionary` | supplied | `/usr/dict/words` |
| `spell-lists` | supplied | American/British/local word additions and stop list are supplied in the reference spell directory. |
| `spell-hashes` | generated | `/usr/dict/hlista`, `/usr/dict/hlistb`, `/usr/dict/hstop` |
| `learn-lessons` | supplied | `/usr/lib/learn` |
| `learn-workdirs` | generated | `/usr/lib/learn/play`, `/usr/lib/learn/log` |
| `formatter-tables` | generated | `/usr/lib/font`, `/usr/lib/term` |
| `formatter-macros` | supplied | `/usr/lib/tmac` |
| `bibliography-data` | supplied | `/usr/dict/papers/Ind`, `/usr/lib/eign`, `/usr/dict/lookall/All` |
| `units-data` | supplied | `/usr/lib/units` |
| `bc-math` | missing | Optional /usr/lib/lib.b is referenced by bc source but absent from the full pinned archive; recover it separately or document the unresolved feature. |
| `fortune-data` | supplied | `/usr/games/lib/fortunes` |
| `quiz-data` | supplied | `/usr/games/quiz.k/index` |
| `v7-source-archives` | supplied | libplot backends contain source in historical .c.a archives; bootstrap extraction must support the historical archive format. |
| `v6-compatibility` | pending_assessment | V6 syscall wrappers are supplied for historical compatibility; determine whether native variants are appropriate. |
| `pdp11-assembly` | pending_assessment | Rewrite or retarget PDP-11 assembly; do not treat successful host compilation as native compatibility. |
| `man-command` | supplied | Original inputs are supplied in the full archive. The original bin/man shell script is inventoried as script.man and remains unported. |
| `manual-pages` | supplied | Original inputs are supplied in the full archive. Install manual sections 0–8 with native-port annotations and formatting support. |

Items previously called missing from the source subset are supplied by the
full archive: original headers, yaccpar, the C lex skeleton, lint descriptions,
formatter macros, six lesson archives, dictionaries/bibliography data, units,
fortune/quiz datasets and original configuration. “Supplied” means reference
inputs are available, not that their native port or installation is complete.
Historical font/terminal tables and spell hashes need native regeneration or
format validation. Optional nrform/ebcform lex skeletons and bc's lib.b remain
absent from the complete archive. Kernel/runtime requirements remain pending
according to their actual Nexix implementation status.

## Coverage validation

```sh
make TOOLCHAIN_PREFIX= test-inventory
```

The read-only checker also validates all 361 roadmap IDs, summary statuses,
component/requirement mappings and task ownership, rejecting omissions and
misleading completion mappings.

The read-only checker verifies all reference bytes against the committed archive
index, catches missing or additional files, and validates ownership of the entire
reference/v7 tree. It checks component IDs, source availability, dependency and
evidence paths, installed-program ownership, source archives and binary-only
records. Claimed native programs must match the build and root-image declarations.

Negative tests cover omitted sources/scripts, concealed components, missing
requirements, duplicate IDs, unsupported port claims, missing program-index
entries, provenance drift and wrong archive hashes. To independently compare
both index and files with the pinned archive:

```sh
python3 tools/import_v7_reference.py /path/to/v7.tar.gz --check
```

A passing check means full archive coverage, byte fidelity and inventory
consistency. It does not prove transitive API compatibility or native functional
acceptance. Source-less historical binaries remain visible blockers.

## Kernel foundation

Kernel reference sources remain under usr/sys/{sys,dev,h,conf}. Nexix adapts
V7 algorithms and behavior around a native x86_64 ABI; PDP-11 startup,
segmentation, assembly and swapping assumptions are replaced. Current foundations
include protected address spaces, processes/signals, native ELF execution, file
descriptors/pipes, one serial TTY, V7 disk encoding and synchronous IDE persistence.
See [architecture](architecture.md), [userland](userland.md) and
[syscall ABI](syscall-abi.md) for implemented behavior and current limits.

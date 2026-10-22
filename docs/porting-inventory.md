# V7 porting inventory (task 21)

Nexix 0.1.0 targets the complete V7 source distribution through native kernel,
userland, toolchain and historical-platform equivalents, with multi-user boot. The first
20 tasks provide a bootable single-user system with persistent IDE storage.
This inventory accounts for the remaining work; it does not implement ports.
See [manual pages](man-pages.md), [ps](ps.md) and [uptime](uptime.md) for the
additional installed manual/monitoring commands and their native adaptations.

The authoritative [manifest](v7-userland-manifest.json) accounts for all
**2,144 distribution files**, grouped into **217 historical components**.
Seven native additions bring the manifest to **224 components** and **36 platform
or supporting-input requirements**. The preserved userland source subset still
contains 1,121 files and its original 174 component records. The full audit adds
installed scripts, additional games, the root diagnostic source, kernel/boot
references, and supporting-data groups. All archive contents are preserved.

**init, sh, echo, cat, ls, pwd, cp, ln, rm, date, touch, stty, tty, mesg, getty, login, passwd, su, newgrp, who, write and wall** are installed native ports.
Task 82 completes the native libc ABI and installs its headers, archives and
startup objects; see [development interfaces](development-interfaces.md). The archive's **213 installed program paths** are listed
separately; scripts and old executables in reference/v7 are not installed Nexix
ports. **13 binary-only program components** are blocked pending source recovery
or an explicitly approved alternative. No historical hardware exception is
silently excluded from the release objective.

The [distribution index](../reference/distribution.json) pins each file's bytes
and original metadata to the recorded archive. See [source provenance](../reference/README.md).

## Manifest contract

The reference inventory still covers 217 historical components. Native-only
additions are recorded separately: `cmd.uname`, `cmd.hostname`, `cmd.clear`,
`cmd.vi`, `cmd.uptime` and `cmd.more` bring the total to 223. The additional traditional-vi requirement records
the separately pinned release and its tested native terminal/editor/recovery port.
`source_availability: native` requires nonempty `native_sources` outside
`reference/`, and empty reference `sources`/`source_files`. Native additions
also require installation outputs, tests and an explicit roadmap mapping.

- Schema version 2 covers reference/v7, with usr/src separately identified.
- The required roadmap_coverage path selects [roadmap-coverage.json](roadmap-coverage.json).
  All components and requirements map to explicit roadmap task IDs; see the
  [coverage/status contract](roadmap-coverage.md). Task mappings describe planned
  responsibility and do not change native implementation status.
- Installed shell scripts preserve reference bytes unless an explicit
  `ported_script` outside the reference tree and `port_notes` document their
  native adaptation. Staging tests compare against that declared active source.
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
373-task roadmap is in [tasks.md](tasks.md). RTC/timestamps/TZ are
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

Independent TTYs, authenticated credentials and the planned kernel device
equivalents are implemented; remaining program consumers retain their tasks. Profiling sampling/startup/monitor records exist.
Kernel exit accounting and parent/child tracing with native register access,
debugger writes, single-step and profil sampling are implemented.
Block/raw files and the multi-volume mount/unmount namespace are implemented.
Programs such as ps/pstat/iostat inspect historical kernel layouts; compiling
their sources alone does not provide a working native equivalent.

<!-- requirements:start -->
| Requirement | State | Supplied inputs / remaining work |
|---|---|---|
| `native-build-tools` | supplied | Host GCC/binutils bootstrap native Nexix ELF builds; retained native tools remain unported. |
| `headers` | partial | Native public headers are installed and tested without host headers. Seven application-owned format headers retain their explicit consumer tasks; see docs/header-coverage.json. |
| `kernel-abi` | supplied | Native V7 process/filesystem/TTY/device/syscall ABI and architecture equivalents through tasks 76-82 are implemented and tested; later programs retain consumer acceptance. |
| `floating-point` | supplied | Native x87/MMX/SSE process/signal state, binary64 helpers, conversion/formatting, libc/libm archives and public math headers are installed/tested; later language/compiler consumers retain their own tasks. |
| `wall-clock` | supplied | UTC wall clock is initialized from RTC at boot; V7 inode timestamps and sync superblock time use the system clock and persist in the existing disk encoding. |
| `timezone` | supplied | Native gmtime/localtime/asctime/ctime, V7 timezone/ftime, and configurable TZ: fixed offsets, one-hour DST with M rules or historical V7 US defaults; unsupported TZ falls back to GMT. |
| `accounts` | supplied | Native account databases, DES authentication, owned homes/profiles, shared V7 temporary files, automatic two-line getty/login, passwd, su and newgrp are installed and tested. |
| `multi-tty` | supplied | Independent COM1/COM2 UARTs, per-line queues/settings, controlling terminals, process groups, signals and flow control pass two-shell acceptance with repeated resource recovery. Getty/login supervision is tracked separately. |
| `accounting` | partial | Kernel exit accounting and privileged acct control exist. Login/init integrate native per-line utmp and append-only wtmp with concurrent authenticated-session acceptance; accton/sa command ports remain pending. |
| `kernel-information` | supplied | Native pointer-free kernel snapshots support installed ps and uptime; pstat/iostat and debugger consumers retain their own tasks. |
| `tracing-profiling` | partial | Native tracing, debugger writes/single-step, profil sampling and monitor/mcount with profiling startup/records exist. ELF64 nlist and shared symbol contracts exist. Symbol consumers and the adb/prof command ports remain pending. |
| `raw-storage` | partial | Block/raw device files and multi-volume mount/umount routing exist; mounted-device writes are guarded, storage-administration command ports remain pending. |
| `historical-devices` | pending_assessment | Native disk/tape/DECtape, TTY/modem/packet and typesetter/printer/plotter equivalents exist; remaining command consumers and final device-node dispositions stay with their owners and task 359. |
| `elf-toolchain` | missing | Retarget retained compiler/assembler/linker and archive/symbol utilities to native x86_64 ELF. |
| `spool-locks` | missing | Historical spool and UUCP configuration fixtures are supplied; native services, ownership and locking remain pending. |
| `yacc-skeleton` | supplied | Native yaccpar adapts the supplied V7 skeleton with bounded stacks, safe empty reductions and tested error recovery (task 170); combined lex/yacc consumer acceptance remains task 173. |
| `lex-skeletons` | supplied | Native ncform adapts the supplied driver (native-width pointer comparisons, uncompressed miss handling; task 171); separately pinned/adapted illumos nrform and tested native extended-byte ebcform equivalent are installed beside it. Exact historical ebcform text remains unrecovered; no mode is waived. Ratfor execution stays in task 173. |
| `lint-descriptions` | supplied | Supplied llib-lc, llib-lm and llib-port are installed in native form by task 262: their definitions are rewritten as prototypes that agree with the native libc headers (lint1 rejects any mismatch), and llib-lm omits erf/erfc, which no native library provides. |
| `word-dictionary` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `spell-lists` | supplied | American/British/local word additions and stop list are supplied in the reference spell directory. |
| `spell-hashes` | generated | Generate hlist/hlista/hlistb/hstop from dictionaries using spellin. Historical installed forms are also supplied; regenerate architecture-dependent formats for native consumers. |
| `learn-lessons` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `learn-workdirs` | generated | Create learn play/log directories and install private tee/lcount helpers. |
| `formatter-tables` | generated | Native terminal/font tables are regenerated and tested with nroff/troff by tasks 265–267; historical binary table layouts are not installed as native objects. |
| `formatter-macros` | supplied | Original tmac macros are installed unchanged and exercised by native nroff/troff; task 281 complete. |
| `bibliography-data` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `units-data` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `bc-math` | supplied | Historical V6 lib.b recovered separately from the V7 source-history snapshot and retained unchanged; all six math function inputs supplied. Native bc/dc -l installation and numeric acceptance remain task 209. |
| `fortune-data` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `quiz-data` | supplied | Original inputs are supplied in the full archive. Native integration, generated formats and functional acceptance remain pending. |
| `v7-source-archives` | supplied | All twelve historical source/lesson archives are extracted and round-trip tested by task 87; plotting/learn consumers retain tasks 283–285/289–293. |
| `v6-compatibility` | supplied | V6-on-V7 application wrapper needs are fulfilled by native V7 interfaces; V6 binary/syscall compatibility is not an application ABI. Every wrapper is mapped in docs/runtime-equivalents.md. |
| `pdp11-assembly` | partial | Native compiler arithmetic/prologues, syscall stubs, kernel machine code and FP/runtime replacements are mapped and tested. Assembly consumers in later application/compiler/standalone tasks still require their own native ports. |
| `man-command` | supplied | Original man script is installed with a native formatter/pager adapter; lookup, modes, errors and serial/VGA navigation are tested by tasks 287/388. |
| `manual-pages` | supplied | All 307 original manual files are installed unchanged with separate native annotations and formatter/lookup acceptance; tasks 286/287 are complete. |
| `traditional-vi` | supplied | Pinned Traditional Vi 050325 sources and tested native vi/ex/view, terminal/library adapters, preservation/recovery and packaging; tasks 374-385 complete for the documented single-byte fixed-geometry configuration. |
<!-- requirements:end -->

Items previously called missing from the source subset are supplied by the
full archive: original headers, yaccpar, the C lex skeleton, lint descriptions,
formatter macros, six lesson archives, dictionaries/bibliography data, units,
fortune/quiz datasets and original configuration. “Supplied” means reference
inputs are available, not that their native port or installation is complete.
Historical font/terminal tables and spell hashes need native regeneration or
format validation. The optional lex and bc inputs absent from the pinned archive
now have [separately recorded source/native-equivalent dispositions](support-inputs.md).
Their full consuming-program ports retain their roadmap tasks.

## Coverage validation

```sh
make TOOLCHAIN_PREFIX= test-inventory
```

The read-only checker also validates all roadmap IDs, summary statuses,
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


Task 65 implements the multiplexor's group/channel creation, tree identifiers,
join/extract/detach and descriptor/inode lifecycle from `usr/sys/dev/mx1.c`,
`mx2.c`, `usr/sys/h/mx.h` and `usr/man/man2/mpx.2`.
See the [native contract and tests](multiplexor.md). Task 66 implements
data/control records, bounded queues, flow control,
wakeups and named-open/listener/attach handshakes. Task 67 adds COM1 TTY
bridging, process-group/signal controls, ioctl request/reply and twelve libc
exports through `<sys/mx.h>`. This does not complete the kernel or libc inventory components.

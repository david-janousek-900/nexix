# Essential V7 commands (tasks 119–138)

Active copies live in `user/v7/cmd`; the reference tree is preserved. Commands
are native ELF64 files installed by the full-image manifest, independently of
the fixed recovery root. Original syntax and statuses take precedence over
modern Unix conventions. Guest fixtures execute installed files on disposable
IDE roots at 16/128 MiB; `make test-NAME` runs each port's acceptance.
Each task has its own implementation/test commit in Git history.

## basename (119)

Retains final-component extraction, optional suffix removal, empty results for
trailing slashes and status 1 plus a newline for missing arguments. Adapted
historical two-argument puts to native puts. No modern option parser is added.

## cal (120)

Retains month/year output, Julian/Gregorian rules and Britain's missing eleven
days in September 1752. Native fixes bound numeric parsing and trim calendar
rows without reading outside the buffer. Invalid dates return 1; usage retains
status 0. Fixtures independently construct modern month/year calendars.

## chgrp (121)

Retains numeric/group-name lookup, multiple operands, owner preservation and
statuses 4 (usage/unknown identity) and 1 (per-file failure). Native checks reject
empty/overflowed 16-bit IDs and skip failed stat calls rather than reuse metadata.
Guest fixtures check actual ownership and non-root permission denial.

## chmod (122)

Retains V7 numeric modes, symbolic classes/operators, mode-copy operations and
umask rules. Renames the historical abs parser to avoid native libc's abs ABI.
Guest fixtures verify modes, set-ID bits, owner/non-owner behavior, multiple
operands and exact usage/invalid-mode diagnostics and statuses.

## chown (123)

Retains numeric/user-name selection, group preservation, multi-file handling
and V7 diagnostics/statuses. Native 16-bit ID bounds and stat failures are
checked before changing ownership. Fixtures inspect UID/GID and non-root denial.

## expr (124)

The retained expr.y precedence/productions are implemented by a native recursive
parser; no target yacc prerequisite is introduced. The original BRE compiler
and matcher remain, with bounded bytecode/captures and empty-backreference safety.
Arithmetic uses checked native long integers; invalid expressions, division by
zero and overflow return 2. Comparisons implement the documented <= relation
(correcting the supplied action's >= typo). Includes match/substr/length/index,
conjunctions, captures and status 0/1 for true/false results.

## file (125)

Retains C/Fortran/assembler, English/ascii, commands, data, special-device and
historical archive/a.out classifications. Adds native ELF64 x86-64 type and ar
recognition with explicit truncation checks. Historical magic uses byte decoding,
not native int overlays. Text scanning has sentinel storage/bounds guards;
-f retains status 1 and accepts a final filename without a newline. Diagnostics
retain the historical classification-command convention (ordinary missing files
are reported on stdout without a failing status).

## kill (126)

Retains numeric -signal syntax, default SIGTERM, multiple PIDs and per-process
errors. Native parsing rejects overflow/trailing junk and signal-only invocations
instead of risking another PID. Fixtures verify signal 0, actual child death,
missing processes, invalid signals and non-root permission checks.

## mkdir (127)

Retains V7 mknod plus explicit dot/dotdot hard links. Installed set-UID root
because the kernel deliberately reserves directory operations for effective
root. access checks use the real caller; new ownership is restored to its
real UID/GID. Bounds, ownership failure and rollback are checked. Guest tests
verify inode links, umask, multi-operand recovery and non-root denial/success.

## mv (128)

Retains hard-link/unlink moves and cp fallback across devices, including explicit
native utimbuf timestamp restoration. Fixes uninitialized destination storage
and bounds pathname construction. Directory moves use caller access checks and
set-UID root directory links; regular-file moves drop privilege. Guest tests
mount a disposable auxiliary ATA filesystem and verify data/times, cross-device
directory rejection, subtree rejection, inode identity and unprivileged renames.

## nice (129)

Retains V7 -increment syntax (including --5 for a negative increment), default
10 and execvp/script fallback. Native increments are bounded to -40..40. The
original behavior of proceeding to exec after a failed nice call is retained.
Guest kinfo observations verify child niceness and parent isolation; fixtures
also cover PATH, scripts, missing commands and usage errors.

## osh (130)

Retains the older shell parser, quoting, positional substitution/shift, command
lists, pipes, subshells, chdir/wait and V7 status accumulation. Tree cells are
native intptr_t; signal handlers, UID/PID widths, historical assignment syntax,
script fallback and path construction are adapted. The absent /etc/glob helper
is replaced internally with bounded, sorted V7-directory expansion supporting
*, ?, [] and quoted literals, including wildcard directory components. Searches
current directory, /bin and /usr/bin as the original shell does; it does not
adopt Bourne shell environment-variable syntax. Guest fixtures cover installed scripts, redirection, pipes, quoting,
argument shifting, globbing, syntax/missing-command and redirection failures.

## rmdir (131)

Retains explicit removal of dot/dotdot and the parent entry, enabled through
set-UID root with real-caller parent write checks. Uses stat(".") instead of
the historical empty pathname, bounds names and directory records, and checks
read errors. Fixtures verify parent link counts, current/nonempty directory
protection, wrong types, missing paths and unprivileged directory removal.

## sleep (132)

Retains integer seconds and the original status-0 arg-count/bad-character
messages; ignores extra operands as the source does. Native overflow is rejected
with status 1. Guest clock-tick observations verify zero and one-second waits.
Full libc alarm/signal compatibility remains separately owned by task 365.

## sync (133)

Retains the argument-independent sync call, with an explicit native status 0.
Guest acceptance invokes the installed command, then cold-restarts a disposable
production-root copy and verifies saved file contents at 16/128 MiB. Nexix's
synchronous storage contract is preserved; no new write-back cache is implied.

## test (134)

Installs the external V7 test and a /bin/[ hard-link alias, independently of the
Bourne shell builtin. Retains string/numeric/logical, -l length, file and terminal
predicates and status 255 for syntax errors. Absolute bracket invocation is
recognized; trailing unparsed arguments are rejected. V7 -f means an existing
non-directory (including special files), not today's regular-file-only test.
Fixtures execute external paths and verify predicates, syntax and denied access.

The embedded shell regression executable is named v7-regression.elf so it cannot
collide with the installed test command artifact v7-test.elf.

## yes (135)

Retains unlimited repetition of y or the first argument, including empty strings
and the historical ignoring of additional operands. Native guest pipe fixtures
check exact repeated bytes, close the reader, and require SIGPIPE termination
and child cleanup rather than leaving an infinite writer running.

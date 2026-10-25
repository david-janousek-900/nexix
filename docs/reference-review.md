# V7 reference and task review

Original review baseline: `fa6076d`. Follow-up reviewed the implementation at
`7bc7c55` and obligation-ownership changes at `c16f201`, against the locally preserved
Henry Spencer V7 distribution. The reference is pinned by
[distribution.json](../reference/distribution.json) to archive SHA-256
`e0fd7bdd046e17de177f5d997acf8123fa43c57b46b032c7337f7666ab69a07e`.
This review uses the pinned snapshot; it does not establish whether other
archives contain additional or recovered sources.

## Distribution coverage

The inventory and provenance checks verify every one of the 2,144 preserved
files, including byte hashes, against the distribution index. All historical and native inventory components and supporting requirements
have explicit task owners. The
installed-program index accounts for 213 historical program paths; this includes
scripts, aliases, helpers and binary-only inputs, not 213 completed native ports.
No unowned distribution file or component was found.

| Reference area | Planned acceptance owners | Review result |
|---|---|---|
| Kernel, machine code and devices | 38–39, 50–78, 362–364, 366 | 113 kernel inputs and all 64 original syscall slots accounted for; behavior remains incomplete |
| Libc, startup objects and V6/runtime assembly | 28, 33–49, 60–61, 67–68, 79, 82, 363, 365 | Checked gen/stdio/sys/crt/csu/v6 responsibilities against libc compall/mklib; sleep needs a separate compatibility task |
| Math, multiprecision, DBM and Fortran libraries | 80–81, 109–118 | Native adaptation and actual compiler consumers remain required |
| Commands, shell, languages, generators and toolchains | 94–108, 119–288, 298–317 | Component owners retained, including build helpers and generated inputs |
| Plot backends and embedded source archives | 87, 289–297 | Six historical source archives remain required inputs; host GNU ar cannot read their format |
| Games with no supplied implementation source | 318–343 | All 13 recovery/port pairs remain explicit blockers |
| Standalone tools and boot support | 344–351 | Native equivalents retained; preserved PDP-11 binaries do not satisfy these tasks |
| Recipes, packaging, manuals, documents and runtime data | 83–88, 286–288, 352–361 | Full distribution installation and native rebuild acceptance remain required |

The original `bin/man` is owned by `script.man`; formatting and installation
are complete under task 287. All 307 manual files are installed and owned by completed tasks 286–287,
and the 174 document files by task 288. Lesson archives, formatter tables,
macros, dictionaries, game data and spool/account configuration are represented
in inventory components and supporting requirements.

## Explicit audit tasks and their resolution

**Task 365: libc sleep compatibility.** `usr/src/libc/gen/sleep.c` saves the
previous alarm, installs a temporary handler, uses setjmp/longjmp and restores
alarm state. Task 365 now preserves the alarm and handler rules using an atomic native
deadline/flag wait. Host and native tests cover zero duration, earlier/equal/later
alarms, unrelated signals, handler restoration and the alarm-before-pause race.
See [kernel timing](kernel-timing.md) for native elapsed-time and return values.

**Task 366: invalid and ignored system calls.** The supplied kernel
`usr/sys/sys/trap.c` implements `nosys` by setting **EINVAL**, and `nullsys` by
doing nothing. Task 366 now returns **EINVAL** for invalid/reserved calls and zero for ignored
slots, distinguishing assigned-but-unavailable calls with **ENOSYS**. No SIGSYS
is generated. Native setpgrp remains an explicit slot-39 adaptation. See the
[syscall ABI](syscall-abi.md) and raw guest acceptance tests.

## Remaining known input and acceptance gaps

The binary-only games, optional bc `lib.b`, optional lex `nrform`/`ebcform`,
and header-only libm gamma remain explicit unresolved inputs. Task coverage
does not resolve them or authorize silently dropping their functionality.
Historical source and lesson archives require native extraction under task 87;
their presence in the outer distribution is insufficient.

The kernel and header maps establish source responsibility. The libc export
checker verifies currently declared native interfaces, not every original
library symbol. Each component still needs a source/function/output review,
native implementation, failure tests and actual consumer acceptance before
its task is marked Done. Aggregate file ownership cannot prove that no smaller
behavioral gap remains; final closure in tasks 355, 358 and 359 must verify the
completed ports against their original sources and recipes.

## Terminology and validation

Roadmap prose, comments, checker diagnostics, test identifiers and coverage
fields now use **task**. Stable numeric IDs and historical Git commits are
preserved. The pristine reference and the technical debugger term
**single-step** are preserved. Coverage schemas now use version 2:
`task_count`, `task` and `tasks` replace their former ownership field names.

`make test-inventory` verifies provenance, inventory ownership, task numbering
and status totals, kernel syscall/source ownership, isolated headers and ABI
records, stdio adapter inputs and the negative audit fixtures. This change
does not modify kernel or libc runtime behavior.

## Follow-up measures (tasks 389–411)

Reference audit totals: **2,144 files; 225 components; 36 requirements; 551 tasks (280 done, 0 in progress, 271 to do).**

No unowned reference component was found. Twenty implemented command components
still declared pending API/ABI/transitive-dependency audits with only completed
owners. Tasks 389–408 now give each audit its own implementation/test/commit
cycle. These records do not establish twenty implementation defects and do not
invalidate the initial acceptance evidence.

Task 409 explicitly owns the later update/cron startup and native crontab/atrun
integration required by the reference rc/crontab workflows. It depends on the
service/administration ports, and remains To do; no daemon is enabled by this
review. Device consumer closure remains task 359; integration and the reported
freeze remain task 358; the thirteen binary-only recovery pairs remain 318–343.

Task 410 requires explicit pending owners for actionable unresolved entries.
The documented date/TZ grammar and reference touch semantics are scope notes,
not incomplete ports. The stale generic manual audit note is removed because
tasks 286/287 already test all original manual assets and native consumers.
Task 411 reconciles completion prose and the supporting-requirement table with
canonical data. Structural tests establish tracking consistency and archive
fidelity; they do not prove all V7 behavior or close the remaining ports.

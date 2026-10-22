# lint (tasks 261–262)

The supplied V7 lint is installed as `/bin/lint` (the adapted `SHELL`
script), `/usr/lib/lint1` and `/usr/lib/lint2`, with native forms of the
description libraries `/usr/lib/llib-lc`, `llib-port` and `llib-lm`.

## lint1

`user/lint/lint.c` is the supplied `lint.c` linked with the retained
native PCC front end (`user/pcc`), exactly as the supplied makefile links
it with the shared mip pass 1:

- `scan.c` is compiled with `-DLINT`, which enables its `VARARGS[n]`,
  `ARGSUSED`, `LINTLIBRARY` and `NOTREACHED` comments and stops `asm`
  text from being written.
- `local.c` is compiled with `clocal`, `andable` and `exname` renamed.
  lint's `clocal` does its conversion checks (`-a` long assignment,
  pointer alignment, function pointer conversions) and then calls the
  native one; lint's `andable` keeps `&auto` as an address operator so
  set/used tracking sees it; lint's `exname` gives `-p` its 6-character
  monocase external names.
- The code-generation hooks are lint's own: `ecode` walks each
  expression for null effects, evaluation order, used-before-set and
  comparisons, and calls, definitions and uses become pass-1 records.

Native differences:

- Sizes stay the LP64 sizes of `user/pcc/macdefs`. `-p` no longer
  switches to PDP-11 sizes, but still requests the portability checks
  and the 6-character external names.
- Names are significant to their full length, so the supplied `%.7s`
  truncation of names in records and messages is gone.
- Type words are the 64-bit mip type words, written in octal.
- A prototype definition ending in `, ...` is treated as `VARARGS n` for
  its n named parameters.
- `(void)` applied to a call uses its value, so a discarded result
  can be marked as intentional.
- Diagnostics use the native front end's `file:line: warning: ...` form.

## lint2

`user/lint/lpass2.c` is the supplied second pass with 64-bit type words,
full-length names (up to 255 bytes) and bounds checks on every field.
The supplied driver piped pass-1 output through `sort -u`. There is no
native `sort`, so lint2 now reads its whole input and sorts it in byte
order, dropping duplicate lines, as `sort -u` did. Already-sorted
input gives the same result. A malformed line ends the pass with
`pass 2 error: ...` and status 1.

## Driver

`rootfs/bin/lint` is the supplied script with these changes:

- Each input is given to `/lib/cpp -C -Dlint` on standard input. It is
  preceded by the native header profile `/usr/lib/pcc.h` (as `pcc` does)
  and by `#line 1 "name"`, so messages name the file as it was given.
- `-I` is added for the input's directory, so its quoted includes
  resolve as they would if cpp opened the file directly.
- cpp writes `/tmp/lint.$$.i` and lint1 runs after it, as `ccom` does under `pcc`. In the supplied pipeline both would be resident at once, and at 16 MiB lint1 could not be loaded. Pass-1 records collect in `/tmp/lint.$$`.
- lint2 reads that file directly; there is no `sort -u` step.

The options are those of the supplied script: `-n`, `-p`, `-lNAME`, the
cpp options `-I -D -O -U`, and the pass options `-h -u -v -x -a -b -c`.

The profile's `_Static_assert` is inert under `-Dlint`. Layouts are
checked by the compilers, and lint's `-h` would otherwise flag each
assertion's constant test.

## Description libraries

The supplied libraries are K&R definitions written for the V7 headers.
Against the native prototypes, lint1 rejects them: for example
`fread(p, s, n, f) char *p;` does not match `size_t fread(void *, size_t,
size_t, FILE *)`.

The installed libraries therefore define the same functions as
prototypes that agree with the native headers. `llib-lc` and `llib-port`
include every header that declares the functions they describe, so any
future disagreement is a lint1 error, and the unit test requires the
libraries to produce no diagnostics. `_iob` is replaced by `stdin`,
`stdout` and `stderr`.

`llib-lm` omits `erf` and `erfc`, which neither V7 nor the native `libm`
provides. `seek` is no longer described, because the native libc has
no `seek`.

## Shared front-end change

`chkpun` now accepts `void *` combined with any object pointer, as C89
does, rather than warning "illegal pointer combination". This affects
both `ccom` and lint1; the PCC unit tests are unchanged.

## Tests

`make test-lint-unit` (`tests/lint.py`) builds lint1 and lint2 with
ASan/UBSan and runs the driver's pipeline on the host. It checks:

- the exact pass-1 diagnostics and the exact lint2 report for two
  inconsistent translation units (`tests/lint/`);
- a clean prototype program against `llib-lc`;
- library descriptions made with `LINTLIBRARY`;
- the `-h -u -x -v -p -a -b` options and the `VARARGS`, `ARGSUSED` and
  `NOTREACHED` comments;
- that sorted, unsorted and duplicated lint2 input give the same report;
- that malformed pass-2 input and malformed C fail cleanly.

`make test-lint` (`tools/lint_smoke.py`) runs the installed driver in the
guest at 16 and 128 MiB, as root and as an ordinary user. It covers the
same reports, `-n`, `-p` and `-lm`, inputs in subdirectories, cpp
options, a missing input, and removal of the temporary file.

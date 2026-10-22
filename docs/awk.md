# Native awk

The supplied V7 awk sources are in `user/awk`. Task 174 ports the front end:
the yacc grammar, the lex scanner, parse-tree construction, the regular
expression compiler, the symbol table, and the generated token and procedure
tables. Task 175 ports the interpreter and data model: values and their
string/number conversion, records and fields, arrays, arithmetic, string
functions, `split`, control flow, and `print` to standard output. Task 176
completes `printf`/`sprintf` formatting, output redirection, files, pipes and
regular-expression matching, and installs `/bin/awk`.

## Generation

`tools/awk_generate.py` follows the supplied makefile on the host:

- `yacc -d awk.g.y`, with `awk.h` copied from `y.tab.h`. The native yacc sources
  are used (`build/host-nx-yacc`). The bootstrap V7 yacc's lookahead sets are too
  narrow for awk's 76 tokens, and its tables differ from the installed yacc's;
- `lex awk.lx.l` with the native `ncform` driver;
- `token.c` rebuilt from `y.tab.h` with the same edits as `tokenscript`. The host
  has no `ed`; the guest test runs the real script with `/bin/ed` and compares;
- `proc > proctab.c`, using a host build of the supplied `proc`.

The generators read copies in `build/gen-awk`, so line directives name
`awk.g.y`, as in a guest build.

## Native adaptations

- Parser values (`YYSTYPE`), `yylval` and a node's `nobj` are `long`, because they
  hold cell, node and DFA pointers as well as token numbers. The node, value
  and table helpers have prototypes in `awk.def`. Unprototyped calls returned
  `int` and truncated `cclenter` and `makedfa` results.
- `error` is variadic, so pointer and floating arguments reach its format.
- `proc` and the generated `proctab.c` use `= {` initializers. `proc` returns 0;
  V7 fell off the end of `main`.
- The generated scanner binds an unset `yyin` to standard input before scanning.
  So `input` selects a command-line program by `lexprog`, and stays at that
  program's terminator: V7 read past it on each call after end of input.
- String and character-class characters are bounded, escapes included. V7 checked
  only ordinary characters against its 150-byte buffer.
- The regular-expression compiler checks the position limit before entering a
  leaf, the state limit before adding a state, and the character class
  terminator. It handles all 256 byte values (V7 had 128 and overran on higher
  bytes). Follow sets shared between leaves, and state 0, which is a follow set,
  are freed once; V7 freed them repeatedly.
- `main` does not append each run to V7's `/usr/pjw/awk/awkhist`.

Program text is 7-bit, as in V7: lex's tables cover 128 characters, and the
scanner echoes other bytes as its default action (see [lex](lex.md)). Tokens
longer than lex's 200-byte `yytext` are split.

## Interpreter and data model (task 175)

Native adaptations:

- Integral values within a native `long` convert to strings as `%ld`, which gives
  the same digits as V7's `%.20g` without depending on libc's 20-digit `%g`.
  Zero keeps `%g`, so `-0` stays `-0`. V7's 32-bit `long` sent larger integers
  through `OFMT`; awk now does that only outside the 64-bit range. `int`, `%`
  and `%=` truncate through `awkint`, which leaves huge, infinite and NaN values
  unchanged instead of converting them out of range.
- `OFMT` must be a single `e`, `f` or `g` conversion; any other format falls back
  to `%.6g`. V7 passed any format to `sprintf` with a double argument.
- String constants are named `"text` in the symbol table. V7 looked them up by
  their text, so `"j"` was variable `j`'s cell, and `"1"` and `1` shared one cell.
- A record ended by end of file is returned. V7 dropped a last line that had no
  newline, and the last paragraph when `RS=""`.
- Unset fields are empty strings, including for assignment and record rebuilding.
  V7 dereferenced their null strings.
- Records, `print` lines and rebuilt records are checked before each copy. V7
  checked after overrunning. The limits are V7's: 512-byte records, 49 fields
  and 29 range patterns.
  `substr` and `split` results are allocated. V7 copied into 100 bytes, and
  `split`'s subscript buffer held only four digits.
- Temporaries are freed after their strings are used (`substr`, `index`, `~`).
  `a = a` copies before freeing. `freesymtab` reads each cell's link before
  freeing it.
- `for (k in a)` copies the subscripts first, so the body may change or `split`
  the array, and it returns a value. A loop variable that is an array is an error.
- Negative field numbers, NF above 49 and more than 29 range patterns are errors.
  `program`, `nullproc` and `instat` no longer return undefined values.
- Syntax errors exit 2; V7 exited 0. `-f` without a file and the usage message
  no longer dereference null pointers.

V7 semantics are kept:
- comparisons are numeric only when both values are numbers, and an uninitialized
  variable is the string `""`;
- `$0` is empty in `END`;
- assigning a field beyond `NF` leaves `NF` unchanged;
- awk reads input even when it has only `BEGIN`;
- options must come before `-f`;
- there is no `^` operator;
- `-Ft` means a tab separator.

`make test-awk-run` builds the whole awk with ASan/UBSan and runs 40 cases with
exact output, error and status:
- values, arithmetic, conversions and `OFMT`;
- fields, `-F`, `RS` (including paragraph mode) and command-line assignments;
- arrays, `split` and `for`-`in`, including changing the array in the loop;
- string functions, temporaries and control flow;
- V7 comparison and record semantics;
- every limit and run-time error.

19 cases where V7 and POSIX agree are also checked against mawk. 300 random
programs over random records must not trigger a sanitizer report. The native awk
runs the same fixtures in 16/128 MiB guests, as root and as an ordinary user.

## I/O, formatting and acceptance (task 176)

Native adaptations:

- `printf` and `sprintf` build their result in a buffer that grows, one per
  call, because arguments may call `sprintf`. Each conversion is formatted with
  its own argument type: `e`, `f` and `g` as double, `s` as string, and `d`, `o`
  and `x` as `long`. Out-of-range values are clamped instead of converted.
  `%%` becomes `%`, and `printf` writes the result rather than using it as a
  format again. V7 wrote into 512- and 100-byte buffers without bounds, kept
  `%%` for that second pass, and so also interpreted `%` in the converted data.
- Only conversions C's printf defines are formatted (flags, width up to a
  million, precision, `l`, letter). V7 did not know `c`, `u` or `*`; those and
  malformed items are copied literally, once. V7 copied them, then repeated their
  letter.
- Output files are matched by name only in used slots (V7 compared null names).
  Files are closed, and pipes waited for, before awk exits, after standard
  output is flushed.
- The grammar's statement error recovery yields an empty statement. V7 linked
  the error token's value into the tree as a node.
- `split` hands a replaced array's cells to `reapsymtab`, which frees them when
  the next statement starts. An enclosing expression may still hold an element,
  as in `a[$1] = split($0, a)`, and V7 awk has no functions, so nothing holds
  one across statements. `substr`, `index` and `sprintf` copy the string they
  keep while other arguments run.

V7 semantics are kept: `sprintf` takes the rest of the print list, so it is not
nested in another list without parentheses. There is no `?:`, a pattern must be
a comparison, match or regular expression, and `]` right after `[^` ends the
class (`[^\]a]` excludes `]`). Up to 10 output files may be open.

`make test-awk-run` adds the following cases to those of task 175:
- `printf` and `sprintf` conversions, flags, widths and errors;
- `>`, `>>` and `|` output, checked through the files and pipes they create;
- 42 regular expressions on 28 lines, agreeing with mawk;
- complete programs: word counts, reports, statistics, numbering, field reversal
  and a password-file filter.

58 cases in all, 25 of them also checked against mawk. The random programs
include random `printf` formats and `sprintf` calls. The installed `/bin/awk`
runs every case in 16/128 MiB guests, as root and as an ordinary user, with
command-line programs, shell pipelines and the manual page. Pipes use `cat`
until `sort` is ported.

## Front-end acceptance

`make test-awk-parse` checks that `tokenscript` reproduces the supplied
`token.c`, and that `proctab.c` maps every token to its procedure and print name.
It then builds `tests/awk_parse.c` under ASan/UBSan. This harness links the awk
objects without `main.o`, parses a program and prints its tree. Regular
expressions print as their DFA's results on fixed probe strings.

Hand-checked trees in `tests/awk` cover operator precedence, all statements,
patterns, ranges, functions, redirections, string escapes and DFAs. Error tests
cover syntax errors and recovery, newlines in strings and expressions, string,
class, expression and DFA limits, and overlong tokens. 400 random programs
must not trigger a sanitizer report.

Disposable 16/128 MiB guests, as root and as an ordinary user, regenerate
`y.tab.c`, `y.tab.h`, the scanner, `token.c` (with `ed` and `tokenscript`) and
`proctab.c` (with a native `proc`) using the installed tools. Each must match the
host output byte for byte. The guests then run the native harness on the
fixtures.

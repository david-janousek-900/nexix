# Native lex

Task 171 installs `/bin/lex`, retaining the supplied V7 scanner generator: the
definition/rule grammar, parse-tree construction, follow-set and DFA state
generation, character-class and transition packing, and C/Ratfor table output.
The active sources are in `user/lex`; the grammar is generated at build time by
the retained host V7 yacc using the native `/usr/lib/yaccpar`.

Native adaptations:

- Parse-tree operands and parser values hold pointers to character classes,
  strings and start-condition lists, so `left`, `right` and `YYSTYPE` are `long`.
  Diagnostics are variadic; obsolete `=+`-style operators and initializers are
  rewritten.
- Generated scanners declare `FILE *yyin, *yyout` without initializers because
  native `stdin`/`stdout` are not constants. `yylex` binds any stream still unset
  to `stdin`/`stdout` before scanning, so programs may still assign either stream
  before calling it.
- Table and tree limits are checked before writing a node, position, state,
  transition or output slot. The supplied code checked after writing, which
  overran its arrays near every limit (for example `-f` with the default `%a`).
  The `%T` packing bound accounts for character codes equal to the table size.
- Several input files are read in order. The supplied code advanced the file index
  twice and skipped every second file. Input is closed once at end of file.
- A rule syntax error that yacc recovers from still makes lex exit with status 1;
  the supplied program reported it but exited 0 with an unusable scanner.
- A specification without a `%%` delimiter reports a syntax error instead of
  writing scanner trailers to a stream that was never opened.
- `NX_LEXLIB` selects another skeleton directory, for host testing only;
  the default remains `/usr/lib/lex`.

Supported interfaces are the V7 options `-t`, `-v`, `-n`, `-f`, `-r` and `-c`;
definitions and substitutions; `%{ %}` code; local declarations at the start of
the rules; start conditions and `BEGIN`; `^`, `$` and `/` trailing context;
character classes, repetition, `{n,m}`, quoted strings and escapes; `yymore`,
`input`, `unput`, `yylineno` and `ECHO`; the `%e %p %n %k %a %o` table sizes; and
`%T` character tables. Undefined definitions, undefined start conditions and
invalid `%` requests remain warnings, as in V7.

## Scanner drivers

`/usr/lib/lex/ncform` adapts the supplied C driver: pointer comparisons against
`yycrank` use `uintptr_t` rather than truncating to `int`, and an unmatched
character in an uncompressed (`-f`) state is pushed back and ends the token, as
in the compressed path. The supplied form fell through and consumed it.
`/usr/lib/lex/ebcform`, selected when a `%T` table declares more than 128 codes,
and `/usr/lib/lex/nrform`, used by `-r`, are the task 88 native/pinned inputs
documented in [support inputs](support-inputs.md).

## Acceptance

`make test-lex` builds the production generator with ASan/UBSan. It generates,
compiles and runs C scanners for start conditions, trailing context, `yymore`,
`input`/`unput`, `{n,m}`, anchors and line counting, in compressed, uncompressed
and enlarged-table forms, and for `%T` tables with ordinary and extended-byte
drivers. It also checks `-t`/`-v`/`-n`, standard input, several input files and
Ratfor generation. Malformed specifications, every table limit, missing drivers,
unwritable output and 400 random specifications must fail or warn without a
sanitizer report. Disposable 16/128 MiB guests compare the installed generator's
C and Ratfor output byte-for-byte with the checked host output. They run native
scanners built from that C, check diagnostics and exercise ordinary-user generation.

The `libln` runtime (`yyless`, `yyreject`, `main`, `yywrap`, `allprint`) and the
`smallex` target are task 172. Task 173 completes combined lex/yacc consumer
acceptance and executes Ratfor scanners with the native Fortran toolchain.
Host GCC remains the bootstrap C compiler for the generated test scanners.

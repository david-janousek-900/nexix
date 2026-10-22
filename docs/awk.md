# Native awk

The supplied V7 awk sources are in `user/awk`. Task 174 ports the front end:
the yacc grammar, the lex scanner, parse-tree construction, the regular
expression compiler, the symbol table, and the generated token and procedure
tables. The interpreter and data model are task 175. I/O, functions and
end-to-end acceptance are task 176, after which `/bin/awk` is installed.
`build/v7-awk.elf` already links every supplied object.

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

## Acceptance

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

# Native V7 C preprocessor

Task 178 installs `/lib/cpp`, bootstrapped with the host compiler and statically
linked against native libc. `user/cpp/cpp.c` retains the supplied Reiser V7
scanner, macro expansion and include machinery; the reference tree is unchanged.
The port uses public native stdio, file descriptors, strings, integer conversion
and variadic diagnostics. It has no runtime shell or external tool dependency.

`user/cpp/expr.c` replaces the original yacc expression evaluator and lexer with
recursive descent. Expressions use LP64 signed/unsigned integer values, C operator
precedence and lazy `&&`, `||` and conditional evaluation. This avoids the original
32-bit arithmetic, hex-digit conversion error and division in skipped branches.
Division by zero, invalid shifts, overflowing literals, malformed expressions and
excessive nesting fail with diagnostics. Arithmetic storage uses unsigned wrapping;
signed comparisons/division and arithmetic right shift follow the native ABI.

The interface retains V7 `-P`, `-E`, `-C`, `-R`, attached `-Dname[=value]`, `-Uname`
and `-Idir` options, stdin/stdout, input and optional output positional files,
quoted/angle includes, object/function macros, undefinition and conditionals.
Quoted includes search the input directory first; `-I` directories precede
`/usr/include`. Native predefines are `unix`, `__nexix__`, `__x86_64__` and
`__LP64__`; PDP-11/VAX predefines are absent. Complete identifiers are significant.
`__FILE__` and `__LINE__` reflect `#line` changes.

This is traditional V7 preprocessing: comments disappear (so `a/**/b` becomes
`ab`), formal parameters substitute within quoted macro bodies, and directives
start with `#` in column one. ANSI stringizing/token-pasting, variadic macros and
`#elif` are outside the supplied interface. `-R` explicitly enables recursive
macro expansion as in V7; the normal mode reports runaway recursion.

Storage is bounded: a macro arena that grows with sbrk to 1 MiB (installed builds use static stdio buffers to keep it contiguous), 4096 symbol slots, ten include levels,
31 macro parameters, fourteen pushback buffers, twenty each `-D`/`-U` options and
eight `-I` directories. Individual scanner tokens/argument lists retain the
native `BUFSIZ` bound. Limits and malformed input report failure rather than
writing outside buffers. The byte 0xfe is reserved for internal macro markers.

`make test-cpp` runs production code with host ASan/UBSan, compares expressions
with an independent host preprocessor, preprocesses every public native header,
compiles and executes emitted C and a real V7 yacc-generated parser, and probes
malformed input and limits. Installed guest tests run at 16 and 128 MiB on
disposable roots, checking exact output, the staged native headers, generated
parser text, includes, stdin, output files, error status and ordinary-user access.
This completes preprocessing; it does not complete either compiler family or
the native toolchain rebuild acceptance task 207.

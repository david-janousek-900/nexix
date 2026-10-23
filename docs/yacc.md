# Native yacc

Task 169 installs `/bin/yacc`, retaining the supplied V7 LALR(1) grammar parser,
closure/lookahead/state generation and table optimizer. The active sources are
in `user/yacc`; the separate equation-grammar host bootstrap remains unchanged.
Native adaptations use 32-bit lookahead words with unsigned masks, explicit state
items instead of integer/pointer overlays, separate optimizer scratch arrays,
LP64-safe variadic diagnostics and checked grammar/name/type storage. The generator
uses native public libc interfaces and no external program. It reads
`/usr/lib/yaccpar` by default; `NX_YACCPAR` can select another template.

Supported V7 interfaces include token declarations and values, literals, precedence,
associativity, start/type/union declarations, empty productions, embedded actions,
error tokens, `-d` headers, `-v` state reports and the historical `-o` compatibility
option. The reference generator explicitly rejects Ratfor yacc output; no such
mode is silently substituted. The medium configuration retains the supplied
limits: 127 terminal slots, 400 productions, 600 states, 200 nonterminals, 49-byte
names and 62 semantic types. Limit failures and I/O failures return nonzero.

`make test-yacc` runs the production generator with ASan/UBSan, compiles and executes
its typed/precedence parser on valid and invalid input, crosses every 32-bit
lookahead boundary with 80 terminals, and checks malformed grammars and storage
limits, output failures and the supplied PCC grammar. Disposable 16/128 MiB guests compare generated C, headers and state reports
byte-for-byte with the checked host output and exercise ordinary-user generation.
Task 170 completes the generated-parser stack/error/overflow acceptance described
below. Task 173 compiles a lex/yacc consumer with the native `cc`; see
[lex](lex.md#native-consumers).

## Native parser skeleton

Task 170 adapts the installed `/usr/lib/yaccpar` while preserving the immutable
reference template. A real sentinel slot replaces pointers formed before the
stack arrays; overflow is checked before a state/value write. Empty reductions
use a zero semantic value rather than reading past a full stack. Error recovery
stops at the real stack base and can safely report debug state information.
The table generator also tests terminal slot 2 (`error`), rather than slot 1
(`$end`), before choosing a default reduction in a recoverable error state.
The supplied code's wrong-slot test otherwise prematurely reduced away the
state needed to recover malformed input.

The extended acceptance compiles generated parsers with ASan/UBSan and tests
pointer-valued union actions, nesting, empty rules, recovery with `yyerrok`, and
stack exhaustion. The same C is bootstrapped against native headers/libc into
private guest ELF fixtures. Each guest generates those C files itself and compares
them byte-for-byte before running the matching native parser, including overflow
and recovery cases. Native cpp also preprocesses generated C. Host GCC remains
the bootstrap C compiler until the compiler/driver tasks are complete; these
tests do not claim task 207's in-guest toolchain rebuild acceptance.

# Native assembler

Task 181 implements the native first-pass parser and symbol/directive model in
`user/as/parse.c` and `user/as/as.h`. It replaces the supplied PDP-11 assembly scanner
with a C implementation for the agreed ELF64 x86_64 toolchain. The immutable
reference sources remain untouched. The encoder is completed in task 182 below; object output and the installed
as/as2 pipeline remain tasks 183–184; this first pass is currently tested
through a private native consumer rather than installed as a usable assembler.

The language uses AT&T operands: `%register`, `$immediate`, symbolic addresses,
indirect `*` operands and displacement/base/index/scale memory addresses. The parser
retains up to three operands per instruction for the encoding pass. It handles
named and single-digit forward/backward local labels, global/local/weak binding,
function/object types, equates, common/local-common declarations, sizes and
current-location expressions. Duplicate definitions fail. Expressions support
unsigned 64-bit constants, unary signs/complement, parentheses, arithmetic,
shifts and bitwise operators. Relocatable expressions retain one positive and one
negative symbol plus an addend; known absolute equates fold immediately.

Directives cover text/data/bss and named ELF-style sections, section flags/types,
previous sections, byte/word/long/quad and floating data, escaped byte strings,
zero/fill space, byte or power-of-two alignment, file metadata and identifiers.
Merge/string section flags are accepted as data descriptions without requesting
linker merging. The parser deliberately rejects unsupported directives, malformed
operands/expressions, invalid alignment/scale, embedded NULs, oversized names,
lines, sources and excessive expression nesting. It is a native compiler assembly
language, not a complete implementation of GNU macro-assembler extensions.
Symbols, section offsets, data expressions and instruction operands remain typed
first-pass records for subsequent layout, relocation and encoding work.

`make test-as-parse` checks production code with ASan/UBSan, validates the same
fixture independently with GNU `as` and `readelf`, and inspects symbol binding,
common size/alignment, RIP/indexed addressing, local labels, size differences,
constant precedence, strings and IEEE floating data. Malformed-input and nesting
probes include deterministic fuzz cases. Private 16/128 MiB guests execute the
same parser and contract checks as root and an ordinary user. This completes the
first-pass task only; no native executable/object generation is claimed yet.

## x86_64 encoding

Task 182 implements `user/as/encode.c` and `encode.h`. The encoder returns bounded
instruction bytes plus typed relocation requests; section layout and ELF emission
remain task 183. It covers the native compiler's general-register integer moves,
arithmetic, tests, multiply/divide, shifts/rotates, extensions, conditional moves,
setcc, direct/indirect calls and jumps, stack/frame operations and system-call
entry. SSE2 scalar/packed moves, scalar arithmetic/comparisons/conversions and x87
loads/stores/conversions, stack operations, comparisons and arithmetic support
the frozen floating ABI. REP byte/quad moves/stores support aggregate copying.
AVX and CPU extensions outside this baseline are rejected.

Encoding checks register widths/classes, high-byte versus REX restrictions,
address bases/indexes/scales, immediate/displacement ranges and the fifteen-byte
instruction limit. It handles ModRM/SIB, mandatory/operand prefixes and extended
registers. Symbolic operands produce ELF64 64/32/32S or PC-relative/PLT32 requests;
PC addends include bytes after the displacement, including trailing immediates.
Symbolic byte/word immediate relocations are rejected rather than mislabeled as
32-bit relocations. Branches use near displacement fields; relaxation is outside
this encoder. Unsupported combinations fail instead of emitting guessed code.

`make test-as-encode` compares exact instruction bytes, relocation kinds, offsets,
symbol names and addends with independently generated GNU objects. It includes
integer/SSE2/x87 examples and hundreds of deterministic general-register and
addressing combinations, all with production ASan/UBSan. Encoded integer and SSE
functions are executed under the host System V ABI. Private native fixtures
compare the complete encoding output and execute those exact encoded functions
at 16/128 MiB as root and an ordinary user. The full first-pass suite is rerun for
x87 register syntax and REP-prefix parsing changes. No installed assembler or
ELF relocatable output is claimed until tasks 183–184.

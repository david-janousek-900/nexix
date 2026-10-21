# Native assembler

Task 181 implements the native first-pass parser and symbol/directive model in
`user/as/parse.c` and `user/as/as.h`. It replaces the supplied PDP-11 assembly scanner
with a C implementation for the agreed ELF64 x86_64 toolchain. The immutable
reference sources remain untouched. The encoder is completed in task 182 below; object output is completed in task 183 below, and the installed as/as2 pipeline
remains task 184; this first pass is currently tested
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
instruction bytes plus typed relocation requests; section layout and ELF emission are completed in task 183 below. It covers the native compiler's general-register integer moves,
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
x87 register syntax and REP-prefix parsing changes. Installed as/as2 integration remains task 184; task 183 completes object output
below.

## ELF relocatable objects

Task 183 adds `user/as/object.c`: checked section layout, local/common allocation,
label/equate/size resolution, symbol tables and ELF64 AMD64 ET_REL output. It
preserves allocated section flags/alignment, separates NOBITS BSS from file bytes,
orders local symbols before global/weak symbols, and emits explicit RELA sections
with symbol indexes, addends and target-section links. Global common symbols keep
size/alignment; local common symbols allocate BSS. Defined local branches resolve
in section; inter-object references retain the ABI's 64/32/32S/PC32/PLT32 kinds.
Current-location/label differences and supported cross-section PC-relative data
are resolved or represented explicitly. Circular/deep aliases, missing local
labels, unsupported symbolic differences, nonzero BSS initialization and oversized
sections/objects fail before replacing an output. Each section and the complete
serialized object are limited to 8 MiB.

`make test-as-object` runs production code with ASan/UBSan. An independent GNU
assembler produces matching text/data/rodata/BSS contents, flags, sizes and
alignment for the acceptance source. `readelf` inspects the complete object, and
an independent host linker links it into an executed C program testing code,
RIP-relative data, calls, aliases, pointers, BSS and common allocation. Repeated
serialization of the same parsed program must produce identical bytes. Negative
fixtures preserve an existing output. Private 16/128 MiB guests produce the exact
same object, exercise installed nm/size/ar/ranlib consumers and run an independently
linked native program using its code. First-pass and encoder acceptance also run
again. The installed as/as2 driver and helper boundary remain task 184.

## Installed assembler pipeline (task 184)

`/bin/as [-u] [-o object] [source ...]` executes the native `/lib/as2`
worker. Both programs are native ELF64 executables. The worker shares parser
state across source files, then encodes and lays out the complete translation
unit before replacing the output. With no sources it reads standard input;
`-- -` also selects standard input. The default output is `a.out`, containing
an ELF64 relocatable object. A leading V7 `-`, `-u` and `--64` are accepted;
undefined external symbols already have global binding under the native ABI.

The historical two-pass interface is preserved through the installed helper,
while its internal representation uses bounded C structures instead of PDP-11
word overlays and temporary intermediate files. Unsupported options and invalid
assembly fail without overwriting an existing object. Multiple files preserve
section state and numeric local labels; diagnostics identify the input file and
line. This assembler supports the documented native C instruction baseline;
GNU macro processing, AVX, TLS and PDP-11 syntax remain outside that baseline.

`make test-as` instruments the real public-driver/helper pipeline with address
and undefined-behavior sanitizers, checks CLI and failure behavior, and tests
installed programs as root and an ordinary user at 16/128 MiB. Produced objects
match the host acceptance bytes; independent linking and execution verify the
code and relocations. The guest also executes the independently linked result.

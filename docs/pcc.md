# Native PCC compiler implementation

Tasks 193–196 are **complete**. The retained frontend, the integer/pointer/control
backend (194) and the call, aggregate, floating-point and variadic backend (195)
use native LP64 and the shared mip contracts. Task 196 installs the compiler as
`/usr/lib/ccom` with the `/bin/pcc` driver and adds the `fort` second pass; and complete supported
V7 source rebuild acceptance remains 207. The private executables are not yet
installed compiler commands.

## Retained frontend and native types

`cgram.y`, `scan.c`, `pftn.c`, `trees.c`, `optim.c` and `xdefs.c` retain the supplied
parser and semantic machinery. The checked V7 yacc generator and parser skeleton
produce the working grammar (eight shift/reduce conflicts). The original
reference files remain unchanged. Explicit nodes, native prototypes and real
varargs replace anonymous overlays and PDP-11 pointer/stack shortcuts.

Types distinguish signed/unsigned long long, long double, void and `_Bool` as
well as all sixteen original basic codes. Layout is native LP64: char/bool 1,
short 2, int/enum/float 4, long/long long/pointer/double 8, and x87 long double 16
bytes with 16-byte alignment. Natural structure/union layout, arrays and fields
use these sizes. `sizeof` returns unsigned long. Void objects and value uses are
rejected; casts, void calls and void return expressions retain their effects.
Boolean conversion tests the full original scalar value, including fractional
floating values. Long-long arithmetic has its own rank; signed long long plus
unsigned long produces unsigned long long on this ABI.

`numbers.c` checks integer bases, overflow and U/L/LL suffix ordering/spelling.
Unsigned bit storage handles folding without signed host overflow. Pointer
arithmetic and differences retain 64-bit intermediates. `real.c` parses decimal
and hexadecimal literals as bounded integer ratios and rounds directly to
binary24/53/64 with ties to even and gradual underflow. It does not parse an x87
literal through binary64. Constant conversion, floating data and shared IR retain
its binary80 significand/sign/exponent; padding is zeroed in emitted objects.
Native math-header infinity/NaN and classification builtins preserve their types
and effects, including signed zero and unordered comparisons.

Tags, ordinary identifiers and members have distinct namespaces. Each aggregate
has its own member identities; references resolve against the selected type.
Rehashing preserves member and function-signature metadata. Exact identifiers
through 255 bytes, initializer bounds, object/frame sizes, line-marker names and
source-line overflow are checked. Constant bounds and native `offsetof`
assertions fold before validation; failed assertions reject the translation.

## Function signatures and source profiles

`declarations.c` records typed and variadic prototypes, unnamed parameters,
array/function parameter adjustments and nested function-pointer signatures.
Definitions bind named parameters in function scope. Redeclarations, duplicate
parameter names, argument counts, callback signatures and promoted K&R
parameters are checked. Calls convert fixed arguments to their declared types
and default-promote unprototyped/variadic arguments. A typed float parameter uses
the native float ABI rather than the legacy promoted-double entry convention.

The default source profile retains V7 identifier-list typedef shadowing and
old assignment spellings. `--native-source` selects typedef-led parameter lists
and modern assignment tokenization (`p=&object`, `n=-3`); normal compound
assignments work in both profiles. This is an explicit compatibility profile,
not a claim of complete C11 or GNU C conformance.

For installed native headers, preprocess the application with
`user/pcc/native.h` included first and select `--native-source`. The adapter
supplies native `offsetof`, checked static assertions and the SysV 24-byte
`va_list` representation. It erases const/volatile/restrict/inline/noreturn
header hints; it does not provide a qualifier-checking language mode. Public
headers and their ABI records are unchanged. The retained compiler emits
conservative loads/stores and does not exploit those erased optimization hints.
The used `returns_twice` attribute is conservative; aggregate `aligned(n)`
attributes actually update size/alignment and validate power-of-two bounds.
Task 196 owns packaging this profile with the installed driver.

`va_start`, `va_end`, `va_copy` and typed `va_arg` parse and check their operands.
Start requires a variadic definition and its last named parameter; argument
requests reject void/array/function types. Explicit shared IR nodes carry these
operations, including aggregate size/alignment; their runtime lowering is
described under the machine ABI below. Unsupported machine operations fail
rather than becoming implicit external helper calls.

The bounded implementation permits 1024 symbols, 1024 expression nodes, 4096
dimension cells, 128 initializer levels, 150 parameters per function, 1024
signature descriptors, 2048 parameter descriptors and 64 KiB of parameter-name
storage. Packed types allow 29 modifiers. Parameter storage is allocated after
startup and names share a bounded arena. IR allocation follows actual tree size;
this keeps the same frontend usable at the 16 MiB baseline. Object/frame tables
have checked signed-int bit offsets; the kernel's 8 MiB executable image limit
is unchanged.

## Shared IR and machine consumers

`ir.c` iteratively lowers retained trees to checked mip nodes with exact names,
byte offsets, field descriptors, native aggregate layout and source lines.
Version 2 tree/stream formats carry the expanded types and ten explicit x87
bytes; they reject version 1 instead of reinterpreting its type packing. The
native object/call ABI remains version 1. See [shared mip](mip.md).

`output.c` selects direct consumption or `--emit-pass`/`--read-pass` in separate
processes. The backend needs no parser symbol/dimension state. Parser and backend
label sequences are independent. Both paths produce identical assembly and
diagnostics. A separate analysis executable links only shared mip services and
consumes the same real compiler passes.

`code.c` and `local2.c` implement native target hooks, checked template/resource
consumption, RAX/RCX/RDX integer operations and stack spills. Frames preserve the
ABI and stay aligned. Calls independently allocate six GP and eight SSE
registers, then ordered stack overflow slots; variadic callees receive the SSE
count in AL. Integer widths, full-width addresses, field stores, short-circuit
control, switch/goto, static data and scalar floating conversions are executed
against independent GCC callers/callees. Exact-size aggregate copies evaluate
addresses once and preserve neighboring objects.

## Aggregate, long-double and variadic ABI

`abi.c` classifies structures and unions from the frontend member tables using
the System V eightbyte rules: INTEGER, SSE, a lone long double (X87 return
class) or MEMORY for objects over 16 bytes, misaligned members and other x87
contents. Bit fields are INTEGER, arrays classify each element and unions merge
members. The packed class word travels in the `reg` field of STARG, STCALL and
aggregate VA_ARG nodes, so a separate backend process needs no parser tables.

Calls stage memory arguments at their final aligned stack location, then
register values and the callee address in one aligned block. A structure is
passed in registers only if all its eightbytes fit the remaining GP/SSE
registers; otherwise it is copied to the stack. Long double arguments use
16-byte-aligned stack slots. MEMORY results pass a hidden RDI pointer to a frame
temporary. Register, SSE and X87 results are stored into that temporary, and the
call yields its address. The frontend reserves temporaries above the current
automatics for each lowered tree and records the offset in the node.

Definitions reverse this ABI: register-class parameters are stored into aligned
frame slots, memory-class and long double parameters use the caller's stack
copy, and structure returns copy exactly the object size. MEMORY returns copy
into the hidden pointer and return it in RAX. Register-class returns load the
RAX/RDX and XMM0/XMM1 eightbytes. A lone long double is returned in ST0.
Falling off the end of a structure function skips the copy.

Long double values use a single-entry x87 accumulator in ST0. Binary
operations spill the left operand to the stack, so the accumulator never holds
two live values. The popping `faddp/fsubp/fmulp/fdivp` forms leave left-op-right,
and comparisons use `fucomip` with the same unordered flags as SSE. Integer
conversions use `fildq`, with a 2^64 correction for unsigned values.
Truncating conversions use an explicit round-toward-zero control word; unsigned
results at or above 2^63 are biased. Unused long double results are popped, and
`return` keeps the result in ST0.

Variadic definitions save all six GP and eight SSE argument registers in a
176-byte frame area. `va_start` stores the named-register offsets, overflow area
and save-area address into the 24-byte `va_list`. `va_arg` handles INTEGER/SSE
scalars, stack-only long double and aggregates. Register-class aggregates are
reassembled in a frame temporary only when all of their eightbytes remain
available; otherwise they come from the aligned overflow area. `va_copy` copies
the full state, and `va_end` only evaluates its operand.

## Acceptance

`make TOOLCHAIN_PREFIX= test-pcc-front test-pcc-integer test-mip test-mip-infra
 test-mip-pass test-inventory` checks production parsing, semantics and shared
consumption under ASan/UBSan and native root/ordinary-user execution at 16/128 MiB.
Every one of the 49 libc headers is separately preprocessed and consumed in both
native configurations using the explicit adapter. A combined header/type fixture
checks prototypes, callbacks, assertions, alignment, void/bool, extended types,
math and variadic IR. Binary80 literals, including subnormals and tie cases, are
compared with GCC's independently emitted bit patterns. Malformed syntax,
invalid bounds/types/signatures/suffixes, source limits and deterministic source
mutations must fail safely.

Native execution checks typed float calls, long-long rank/arithmetic, boolean
conversions, math predicates and existing integer/floating/call/aggregate-copy
cases through cpp -> PCC -> as -> ld, including direct/split-pass equivalence.
Ordinary translation units bound native assembler working sets. Short `ld -r`
groups keep final link commands within the terminal line limit while preserving
all execution cases. `make TOOLCHAIN_PREFIX= test-pcc-abi` compiles `tests/pcc_abi_fixture.c` with the
sanitized compiler in direct and split-pass modes. It links the result with GCC
callers and callees (`tests/pcc_abi_driver.c`) and repeats the cases through a
GCC-compiled oracle. Cases include every eightbyte class pattern, register
exhaustion, hidden result pointers, x87 arithmetic, comparisons, conversions,
calls and predicates, and variadic integer, SSE, long double and structure
arguments, including va_copy and forwarded va_list. In the guest, native cpp,
PCC, as and ld rebuild the same fixture in bounded units with exact assembly.
Execution runs as root and as an ordinary user at 16/128 MiB. These checks do
not cover the installed driver or full V7 rebuild work. Git history records the
implementation and test evidence.

## Installed driver and alternate targets (task 196)

`/usr/lib/ccom` (`user/pcc/ccom.c`) keeps the supplied interface: it reads
preprocessed C from standard input or a file and writes assembly to standard
output or a file. `-p` calls the native `mcount` on function entry; `mcount`
preserves every register, so arguments survive. `-l` writes
`# line N, file F` before each expression, which pcc requests for `-S`. `-X`
flags select the retained front-end traces. `--native-source`, `--emit-pass`
and `--read-pass` are native options. The mip makefile's `comp` target is the
same compiler.

`/bin/pcc` (`rootfs/bin/pcc`) is the supplied script with native paths. Each
source is preprocessed through a wrapper that includes the native profile
`/usr/lib/pcc.h` (an installed copy of `user/pcc/native.h`, checked identical
by the tests) before the absolute source path, so quoted includes still search
the source directory. ccom always uses `--native-source`. `-O` runs `/lib/c2`.
`-X` formerly named Steve Johnson's experimental compiler; natively it selects
the split two-pass form of the same compiler. `-c`, `-S`, `-p`, `-D`/`-I`/`-U`,
assembly inputs and linking with `/lib/crt0.o` and `/lib/libc.a` are as
supplied. A failing pass leaves no `a.out`.

`fort` (`user/pcc/fort.c`, the mip/pcc `fort` target) is the second pass for
the Fortran front end's SCJ interface. Its native intermediate format uses
64-bit words; the header comment in `fort.c` defines it. Frame and argument
registers become frame-relative names. Expressions are compiled by the same
backend, and `FRBRAC` emits the frame size. The supplied `convert` and
`fconvert` notes, which describe source edits between the one-pass, two-pass
and Fortran builds, are replaced by these explicit output contexts.
`user/mip/g` is the developer search script with native file names.

`make TOOLCHAIN_PREFIX= test-pcc-driver` covers:
- the ccom options under sanitizers;
- fort fed by an independent Python producer: functions, loops, calls,
  doubles and frame-register addressing, linked with a GCC driver, plus
  malformed streams;
- in the guest at 16/128 MiB, as root and as an ordinary user: pcc multi-file
  builds with `-c`, `-O`, `-X`, `-S`, `-p`, `-D`/`-I` and `.s` input, compile
  errors, and fort output assembled, linked and run.

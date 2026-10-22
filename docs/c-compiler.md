# Native V7 C compiler (c0, c1, c2)

Tasks 197 and 198 retarget the supplied Ritchie compiler passes in
`reference/v7/usr/src/cmd/c` to native AMD64. The installed passes are
`/lib/c0` and `/lib/c1`. Both are bootstrapped with the host compiler and
statically linked against native libc. The reference tree is unchanged.
Task 199 completes floating-point values, structure values and the System V
ABI with variadic definitions. Task 200 adds the c2 optimizer (`/lib/c2`), fc1
(`/lib/fc1`) and the native cvopt command. The `cc` driver is task 201.

## Pass interface

`c0 source temp1 temp2 [profile]` and `c1 temp1 temp2 output.s` keep the
supplied command lines and the split between expression/code and string files.
The PDP-11 passes exchanged 16-bit words. `user/c/cpass.h` defines the native
contract instead: every item is a 64-bit little-endian word. Operator words
carry a marker in their upper bits, as the original 0376 byte did. Names and
floating literals are NUL-terminated strings. Constants, offsets and types keep
native widths, and a long constant is one word rather than two halves. Native
additions are explicit operators: register parameter homes, the variadic save
area, va_start/va_arg/va_end/va_copy and source file names. c1 rejects unknown
operators, unmarked or truncated files and malformed expression stacks.

## c0: native widths and types

`user/c/c00.c`–`c05.c` keep the supplied lexer, the priority-driven expression
parser, conversion insertion, constant folding, declarations, statements and
tree output. The type word keeps the original packing, with a four-bit basic
type and two bits per pointer/function/array level. Native layout is LP64:

| Type | Bytes | Align |
| --- | --- | --- |
| char, unsigned char | 1 | 1 |
| short, unsigned short | 2 | 2 |
| int, unsigned, float, enum | 4 | 4 |
| long, unsigned long, pointers, double | 8 | 8 |

Structures and unions use natural member alignment, and their size is rounded
to their own alignment. Bit fields pack into 32-bit units (or 8-bit units for
char fields). A field that does not fit starts a new unit, as in the supplied
`align`. Initialized data pads holes from the recorded member offsets.

The conversion table gains a separate class for long. Integer-to-pointer
assignments widen, pointer-to-integer assignments truncate, and pointer
differences are long. Pointer arithmetic scales a widened index. Integer constant
folding is 32-bit two's complement, with explicit unsigned division, shifts
and comparisons. Decimal constants above INT_MAX become long; U, L and UL
suffixes are accepted. Identifiers are exact up to 255 bytes.

V7 semantics are retained deliberately. Old assignment operators (`=+`, `=-`, and so on)
are recognised, with V7's warning when `=-`, `=&` or `=*` lack a following
space, so write `x = -1`. Initializers may omit `=`. `\/` and `/\` are max and
min. Plain int bit fields are unsigned, and `char` is signed. A cast from char
to unsigned masks to 8 bits. `switch` requires an int, and members may be used
through pointers of other types.

### Native header profile

The installed libc headers are modern C. c0 accepts the constructs they use
without a source adapter:
- `void`, `signed`, `short`, `unsigned char/short/long` and `long long`
  (as long);
- `const`, `volatile`, `restrict` and `inline` qualifiers, and GNU
  `__attribute__` lists, all ignored;
- prototypes with named or unnamed parameters, `(void)` and `...`;
- `__builtin_offsetof`, checked `_Static_assert`, and `__builtin_va_list`
  with the va builtins.

Prototype parameter types declare the parameters of a definition. For a direct
call they also convert arithmetic arguments to the parameter types, and a float
parameter receives a float. Old-style calls follow the V7 rules. c1 sign- or
zero-extends every integer argument to 64 bits, so int values passed to long or
pointer parameters behave. Argument count mismatches against a native
prototype are warnings, since V7 sources predate the headers. `long double`
maps to double. Each structure has its own member names,
as the headers require. A member reference through a non-structure pointer
falls back to the first declaration of the name, the V7 rule. Conflicting
function declarations between V7 sources and native prototypes are warnings.
All 49 libc headers are consumed by the tests.

## c1: the table-driven AMD64 backend

`user/c/c10.c`–`c13.c` keep the supplied reader, the expression optimizer
(commutative-tree reassociation, distribution, constant folding, power-of-two
strength reduction, field rewriting, `reg = x+y` reordering, postfix delay),
the Sethi-Ullman degree calculation and the `rcexpr`/`cexpr`/`match`
machinery over the regtab, efftab, cctab and sptab code tables.

Register numbers name the scratch registers rax, rdx, rcx, rsi, rdi, r8, r9 and
r10. Floating values use the same numbers as xmm registers and are held as
doubles, as on the PDP-11. c0's register variables use the callee-saved rbx and
r12–r15. r11 and xmm15 are template temporaries. Long values need one
register, so the PDP-11 register pairs, odd registers, long helper routines and
long-comparison sequences are gone. Optimizer rewrites that only suited the
PDP-11 are replaced by native forms:
- `&~` via `bic`, and right shifts as negative left shifts;
- int*size pointer conversion and big-endian long halves;
- floating-point bit tricks and the autoincrement and deferred addressing
  modes.

`user/c/table.s` is a new AMD64 table in the supplied cvopt description
language. Templates use AT&T syntax with the original macros: operand
addresses, instruction mnemonics, size suffixes, subtree evaluation into
registers or onto the stack, register names and offsets. Native additions to
the macros:
- `R.q`, `R.l`, `R.1` and similar select a register width;
- a `p` shape flag excludes immediate operands;
- `Y` escapes name machine sequences in `c10.c`: loads with extension,
  constants, division and shifts with their fixed registers, popping stacked
  operands, conversions, floating negation and max/min.

At build time `user/c/cvopt.c` converts the table into ELF assembler data. It
keeps the input language but adapts it to the native table:
- `struct optab` is four bytes, padding and a pointer;
- strings go to `.rodata` and the lists to `.data`;
- the list buffer is in memory instead of `cvopt.tmp`;
- macros and `%` are recognised only in shape and template lines, so the table
  can name `%registers`.

Calls evaluate each argument into its System V location in one aligned block.
Stack arguments go at the bottom, register arguments are staged above them,
and the callee address is last. AL carries the vector register count. Live
scratch registers are saved around calls. Narrow return values are extended.
Functions save the callee-saved registers. They store incoming register
arguments into home slots after the frame has been allocated, keeping V7's
branch to the frame-size code at the end of each function. Absolute 32-bit
addresses are used for externals and statics, because the native link places
user text below 2 GiB. Switches use V7's direct, compare-chain and hashed jump
table forms. Floating comparisons branch correctly for unordered values.

The installed passes grow their arenas with `sbrk`, as in V7. stdio uses static
buffers so the break stays contiguous. Symbol names live in a bounded pool, so
the passes fit the 16 MiB baseline.

## Floating-point values and structures (task 199)

Floating values are doubles in xmm registers. float objects are converted with
cvtss2sd/cvtsd2ss on load and store. Constants are read-only doubles.
Comparisons use ucomisd, and branches treat unordered results as satisfying
only `!=`. Conversions use cvtsi2sd and cvttsd2si; unsigned 64-bit values take
the split/bias sequences. A function returning float returns a single, and the
caller widens it. K&R float parameters arrive as doubles (the V7 rule).
Prototype float parameters are stored as floats.

c0 classifies structures and unions into System V eightbytes from their
member layout. The class word travels with every structure argument, structure
call and structure-returning function:
- Arguments go to registers only when all their eightbytes fit; otherwise they
  are copied to the stack area.
- Structure parameters are stored into home slots, or addressed in the
  caller's area.
- Calls returning a structure use a frame temporary allocated by c0; a
  MEMORY-class result passes its address as the hidden first argument.
- Returns copy the exact size into the hidden pointer, or into a staging
  temporary that is loaded into RAX/RDX and XMM0/XMM1.

V7's static result area (`sfuncr`) and the PDP-11 helper routines
(`csv`/`cret`, `lmul`/`ldiv`/`lrem` and their assignment forms, `fltused`)
have no native counterpart: the native code is generated inline.
Variadic definitions save all argument registers. `__builtin_va_arg` handles
integer, pointer and floating arguments; structure va_arg is rejected.

## c2: the object code improver (task 200)

`user/c/c20.c` and `c21.c` keep the supplied optimizer's design. It reads one
routine at a time, up to `.globl`, into a doubly linked node list. It counts
label references (including `.quad .Ln` jump-table entries) and iterates until
nothing changes. The jump optimizations are as supplied:
- redirecting jumps to jumps, and deleting code after unconditional jumps and
  `ret`;
- removing jumps to the next instruction and unreferenced labels;
- reversing a conditional branch over a jump, and cross-jumping (merging
  identical code that precedes jumps to the same place);
- code motion, which removes V7's jump to the frame-size code at the end of
  each function and back;
- loop inversion, common sequences before jumps, and reordering branches for
  locality;
- moving data sections ahead of the code, so constants emitted inside
  functions do not interrupt the jump graph.

The input is native AT&T assembly from c1 or the portable compiler.
Instructions are kept as text, while jumps, labels and segment directives are
parsed. The fourteen conditional jumps, including the parity and sign jumps
used by floating comparisons, have exact inverses. Directives are never
deleted or moved. New labels are numbered from 1500000000, above c0, c1 (from
1000000) and the portable compiler.

The PDP-11 register-content tracker (`rmove`) becomes a conservative AMD64
equivalent: a register known to hold a memory operand of the same width makes
an identical later load redundant. Stores to any memory, labels, calls and
unlisted instructions forget everything. Writes to a register forget it and
every address formed from it. The supplied `sob` loop instruction,
PC-relative literals, `cfcc`/`sxt`/`tst` and constant-compare rewrites have no
AMD64 counterpart and are omitted. `c2 [-] [input [output]]` keeps the
supplied interface; `-` prints the statistics.

## fc1 and cvopt

`/lib/fc1` is the same pass as `/lib/c1`, linked with the floating-point
startup `fcrt0.o`. V7's fc1 linked a floating-point simulator; Nexix requires
hardware SSE2, so the two passes produce identical code. The native cvopt
command (`build/v7-cvopt.elf`, a build helper with no installed path) converts
the table to the same bytes as the bootstrap build's host cvopt.

## Acceptance

`make TOOLCHAIN_PREFIX= test-c-compiler` runs:
- the passes under ASan/UBSan, with `tests/c_fixture.c` (integer, pointer,
  control-flow, switch, call, structure and bit-field cases) linked and run
  through both GNU as and the native assembler, and compared line by line
  with the same source compiled by GCC;
- the sbrk-arena build, which must produce identical intermediate code and
  assembly;
- c2 on every compiled fixture and V7 source: optimized code must be shorter,
  assemble with GNU and native as, and behave exactly like unoptimized code;
- `tests/c_float_fixture.c` against its GCC-compiled oracle;
- `tests/c_abi_fixture.c` (every eightbyte class pattern, register exhaustion,
  hidden result pointers, float parameters and results, variadic definitions
  and va_copy) linked with the GCC caller/callee `tests/c_abi_driver.c`;
- the V7 dialect fixture `tests/c_v7dialect.c` against hand-computed results;
- every libc header, and 90 supplied V7 command sources, through c0, c1 and
  the native assembler;
- malformed sources, truncated or garbage intermediate files, 600
  deterministic source mutations and 300 mutated assembly inputs to c2, which
  must fail without sanitizer reports;
- in the guest at 16 and 128 MiB, as root and as an ordinary user: native
  cpp, c0, c1, as and ld compile the integer, floating and ABI fixtures in
  bounded translation units, the dialect test and the supplied `echo`, `wc`
  and `cat`, and the results run with the expected output. The fixtures also
  run after `/lib/c2`, `/lib/fc1` output is compared with `/lib/c1`'s, and the
  native cvopt reproduces the generated table.

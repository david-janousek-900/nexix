# Shared portable compiler infrastructure

Task 192 is **in progress**. The first native slice is `user/mip/manifest.h`,
`mip.h` and `common.c`; it is a private compiler library, with no installed
compiler command. The reference tree remains unchanged. Native ABI version 1
is defined in [toolchain-abi.md](toolchain-abi.md).

The supplied manifest's operator numbers, basic type numbers and operator flags
are retained. All 74 entries in `common`'s `indope` table retain their operator,
name and flags, including historical display names. `mip_op_flags` and
`mip_op_name` replace unchecked global table indexing with checked lookups.
Invalid operators return zero/null. Legacy macros in the manifest remain for
source comparison; native consumers use the checked functions in `mip.h`.

The native type word is unsigned 64-bit, with the same four basic-type bits and
two bits per pointer/function/array modifier. Checked operations reject holes,
invalid modifiers, removing a missing modifier and overflowing thirty modifier
levels. Scalar size/alignment follows LP64: char 1, short 2, int/enum/float 4,
long/pointer/double 8 bytes. Function, array and aggregate layouts require the
future front end and return errors from the scalar query. The V7 basic-type set
has no long-long or long-double codes; those extensions remain with task 193.

Integer constant storage retains all 64 bits, including unsigned long maxima.
Offsets are signed 64-bit byte offsets; alignment uses checked unsigned 64-bit
byte arithmetic. Explicit node members replace the historical anonymous union
layouts. Exact names up to 255 bytes replace eight-byte truncation. A bounded,
caller-owned arena uses indices instead of pointer/integer casts, accepts children
only from earlier nodes, and rejects malformed arity, unterminated names, invalid
types and invalid aggregate alignment. Failed construction leaves its output
and arena unchanged. There is no allocation, recursion or serialization in this
slice. The arena permits shared subexpressions; it does not yet perform semantic
type checking or implement optimization, register allocation or code generation.

## Remaining task 192 work and source ownership

| Supplied files | Native work and remaining owner |
|---|---|
| `manifest`, `common`, `comm1.c`, `fcomm2.c` | Operator/type/tree contracts started here; diagnostics, traversal and frontend/backend consumer integration remain in 192. |
| `mfile1`, `mfile2`, `xdefs.c` | Replace anonymous node overlays, symbol offsets and target-dependent shared state in 192; actual front-end semantics in 193. |
| `allo.c`, `match.c`, `reader.c` | Native register model, instruction templates, rewriting and checked pass interface remain in 192/194–195. |
| `cgram.y`, `scan.c`, `pftn.c`, `trees.c`, `optim.c` | Front-end parsing/semantics and optimization integration remain in 193, using the shared contracts from 192. |
| `makefile`, `convert` | Prototype build and one-pass integration instructions; native private test build begins in `tools/mip.mk`; complete compiler build remains in 192/196. |
| `fort.c`, `fallo.c`, `fmatch.c`, `freader.c`, `fcomm2.c`, `flocal2.c`, `forder.c`, `ftable.c`, `fconvert` | Supplied alternate Fortran adapter/wrappers remain in 192/196; binary pass interface must be explicitly encoded, never native struct overlays. |
| `g`, `INDEX` | Reference search helper and source index; developer aids, not installed compiler commands. Native source inspection uses repository search. |

No task is complete based on this private library alone. Compiling C programs
through the shared code and PCC backend remains a required integration gate;
linker tasks 185–187 also remain pending.

## Acceptance evidence

`make TOOLCHAIN_PREFIX= test-mip` compares every supplied operator record, runs
production code under ASan/UBSan, then executes the same contract fixture in
16/128 MiB guests as root and an ordinary user. It checks LP64 scalar layouts,
pointer-to-function layout, maximum pointer nesting and overflow, malformed packed
types, large alignment/offsets/constants, exact long names, child/arity validation,
aggregate alignment and full-arena failure without mutation. Leak detection is
disabled because the managed executor uses tracing; this library allocates no
memory. Commit evidence is recorded in Git history.

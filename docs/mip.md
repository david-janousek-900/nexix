# Shared portable compiler infrastructure

Task 192 remains **in progress**. Native shared services now cover types,
operators, bounded IR, traversal/copying, a checked binary pass interface,
variadic diagnostics, table matching/expansion and generic register/temporary
allocation. They are exercised by private native compiler consumers, including
an as/ld executable pipeline. The retained PCC semantic front end now consumes these contracts (task 193);
machine-code and alternate/shared-consumer integration is still required. There is no installed C compiler yet. Linker tasks 185–187 are
complete; compiler front ends/backends retain tasks 193–201 and 207.

The reference tree remains unchanged. The code in `user/mip` adapts the supplied
manifest/common/mfile2 interfaces; [toolchain ABI version 1](toolchain-abi.md)
defines the native machine contract. All 74 `common` operator entries retain
their exact numbers, names and flags. Checked lookups replace unchecked global
array indexing. Legacy macros remain for source comparison; consumers use the
checked APIs rather than anonymous node overlays or pointer/integer casts.

## Types and IR

Types retain the original four basic-type bits and two bits per pointer,
function or array modifier in a 64-bit word. Operations reject holes, invalid
modifiers and overflow beyond thirty levels. Scalar layouts follow LP64:
char 1, short 2, int/enum/float 4, long/pointer/double 8 bytes. Arrays, aggregates
and function layout need front-end context. Long-long/long-double language
extensions remain with task 193. Constants retain all 64 bits; offsets are signed
64-bit byte offsets. Exact names may contain up to 255 bytes without truncation.

Explicit nodes hold child indices, source line, register/label identifiers,
aggregate size/alignment and separate field width/shift, including 64-bit fields.
Children must refer to earlier nodes; the bounded arena is a DAG. Malformed
arity/types/names/aggregate alignment/fields/register indices and exhaustion
fail without mutation. Traversal is iterative, left-before-right postorder,
visiting shared nodes once. Copying retains sharing and only reachable nodes,
remaps indices, rejects overlap and leaves the destination unchanged on failure.
The maximum is 65,536 nodes, subject to actual guest memory availability.

## Pass format and diagnostics

`NXMIP` version 1 is an explicitly encoded little-endian interface, shared by
one-pass/alternate consumers instead of historical pointer overlays and native
Fortran `long` records. Its 16-byte header contains eight magic/version bytes,
32-bit count and root. Each node has 78 fixed bytes followed by its counted name:
operator32, type64, left32, right32, value64, signed-offset64, IEEE-double bits64,
aggregate-size64, alignment32, register32, label32, field-width32, field-shift32,
source-line32 and name-length16. No padding, native struct sizes or host pointer
values appear. Decoder checks magic/version, count/root, exact payload length,
strings, tree construction and capacity before publishing nodes. Truncated,
malformed and oversized input leaves arena/output unchanged. Limit: 32 MiB.

Diagnostics take real native varargs and report file/line/severity through a
caller-owned FILE. Error limits, fatal state and output errors return control to
the driver rather than exiting inside a shared library. Original PDP-11 stack
argument shortcuts are not retained.

## Templates and resources

`templates.h` retains the supplied operator classes, cookies, shapes, type masks,
needs and rewrite flags. Ordered matching applies the original operator-class,
shape and type-mask rules, then tries available resources. An unavailable entry
falls through to the next candidate; rewrite entries return their description
without allocation. Constants 0/1/-1, names, memory, registers, fields, indirect
operands, pointer-to-array element types and temporary/address shapes are checked.
Table expansion retains A/C/I/U/B operand escapes, L/R/1/2/3 selection, O/Z target
escapes, F suppression, T and S/H/M/N/L metadata. A bounded callback interface
replaces unchecked printf-template substitution. Physical instruction selection,
target escape implementations and call conventions remain tasks 194–195.

Register tables describe up to 64 registers and symmetric alias masks. Reference
counts prevent unsafe allocation of live aliases. A/B scratch classes, three
result resources, permitted operand sharing and aligned temporary slots adapt
`allo.c`'s resource model to native widths. Failed allocation/matching restores
resource state; temporary alignment/overflow and frame bounds are checked.
Target-specific register tables and spill/order decisions remain with backends.

## Source and prototype ownership

| Supplied files | Native adaptation / remaining owner |
|---|---|
| `manifest`, `common`, `comm1.c`, `fcomm2.c` | Native manifest/type/operator/arena services in `manifest.h`, `mip.h`, `common.c`; iterative traversal/copy in `trees.c`; real varargs in `diagnostics.c`. Consumer integration remains 192/193/196. |
| `mfile1`, `mfile2`, `xdefs.c` | Explicit IR/context interfaces and table constants in `mip.h`/`templates.h`; front-end symbol/scope/dimension state remains 193. |
| `allo.c`, `match.c`, `reader.c` | `allo.c`, `match.c`, `trees.c` implement checked resources, table selection/expansion and pass records; target ordering/spilling and front-end integration remain 194–196. |
| `cgram.y`, `scan.c`, `pftn.c`, `trees.c`, `optim.c` | Actual front-end parsing, semantics and optimization remain 193. |
| `makefile`, `convert` | `tools/mip.mk` builds shared native modules/private consumers. One-pass conversion instructions map to explicit shared contexts; complete compiler prototype remains 196. |
| `fort.c`, `fallo.c`, `fmatch.c`, `freader.c`, `fcomm2.c`, `flocal2.c`, `forder.c`, `ftable.c`, `fconvert` | The encoded common pass and shared resources replace old binary overlays; actual alternate Fortran adapter/target consumers remain 192/196 and 203–206. No unused wrapper is claimed to be a compiler. |
| `g`, `INDEX` | Search helper and source index are developer aids; repository search replaces fixed `/usr/src/cmd/mip` paths. |

## Acceptance evidence

`make TOOLCHAIN_PREFIX= test-mip test-mip-infra test-inventory` preserves and checks
all supplied operator entries. Host ASan/UBSan fixtures cover LP64 layouts,
maximum modifiers, arithmetic/arena bounds, register aliasing, failed-candidate
rollback, type/shape/operator classes, rewrite entries, bad escapes, shared-tree
copy/traversal, truncated/mutated wire data and real diagnostic arguments.
Python independently decodes the pass fields and compares exact generated
assembly accepted by GNU as. Host traversal stress uses 16,384 nodes; guest
stress uses 1,024 to fit the 16 MiB multi-user baseline.

At 16/128 MiB, root and ordinary users execute the same shared services, compare
exact serialized/generated bytes, assemble generated code with installed as,
link it with native crt0/libc and a bootstrap C caller using installed ld, then
execute and verify the result. This proves the shared template/pass pipeline;
it does not replace the still-required C-source/PCC/lint/backend acceptance.
Commit/test evidence is recorded in Git history.

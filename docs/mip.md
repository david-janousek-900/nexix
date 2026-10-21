# Shared portable compiler infrastructure

Task 192 is **complete**. Shared native compiler services cover types/operators,
bounded IR, iterative traversal/copy, encoded tree and streaming pass interfaces,
variadic diagnostics, table matching/expansion and register/temporary resources.
PCC uses these contracts for real native code generation in both direct and
split-pass modes. A separate analysis consumer links only the shared services;
an independent producer exercises the same stream contract. The complete C,
lint and Fortran commands retain their own tasks; they are not required outputs
of this shared infrastructure task and are not claimed as installed tools.

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
replaces unchecked printf-template substitution. Physical instruction selection and target escape implementations belong to the
backends. PCC integer emission (194) and scalar/call increments (195) now consume
these contracts; the remaining machine ABI work stays in 195.

Register tables describe up to 64 registers and symmetric alias masks. Reference
counts prevent unsafe allocation of live aliases. A/B scratch classes, three
result resources, permitted operand sharing and aligned temporary slots adapt
`allo.c`'s resource model to native widths. Failed allocation/matching restores
resource state; temporary alignment/overflow and frame bounds are checked.
Target-specific register tables and spill/order decisions remain with backends.

## Streaming consumer interface

`pass.h`/`reader.c` replace the supplied reader/fort native-long records with an
explicit stream. `NXMPS` version 1 starts with a 16-byte header: eight magic/version
bytes, little-endian native ABI ID 1, and a zero reserved word. Each record has
three little-endian 32-bit fields: kind, initializer width and payload length.
Kinds are END (0), literal target text (1), expression (2) and initializer (3).
Text has no embedded NUL and is bounded to 64 KiB per record. Trees contain the
checked `NXMIP` payload above. Initializer widths are 8/16/32/64/128 bits; other
records have width zero. A stream is bounded to 64 MiB, must end with an empty END
record and physical EOF, and rejects unsupported versions, ABI IDs, reserved
fields, kinds, widths and excessive/truncated lengths. Backend support for a
particular initializer or expression type is a separate target decision.

A writer has caller-owned context, a sticky error latch and explicit completion;
failed output cannot be mistaken for a completed stream. The reader validates
an entire record before dispatching callbacks and frees borrowed buffers/arenas
after each call. It stops on callback or I/O failure. Earlier callbacks may have
run when a later record fails; a driver must honor the returned failure and must
not publish its partial assembly as a successful compilation. Analysis clients
share checked IR/traversal without target code or parser globals.

`user/pcc/output.c` provides the direct/stream output context. Private
`pcc-backend-test --emit-pass input.i` performs retained parsing/semantics and
serializes target text and expression/initializer IR. A fresh
`pcc-backend-test --read-pass input.pass` generates assembly without invoking the
parser or retaining its symbol/dimension tables. Backend-local labels have a
separate sequence from parser labels, so the producer never needs to execute
code generation to reserve label numbers. Both paths produce identical assembly.
These private test switches are infrastructure consumers, not the installed PCC
driver promised by task 196.

## Source and prototype ownership

| Supplied files | Native adaptation / remaining owner |
|---|---|
| `manifest`, `common`, `comm1.c`, `fcomm2.c` | `manifest.h`, `mip.h`, `common.c` provide native type/operator/arena contracts; `trees.c` traversal/copy and `diagnostics.c` replace overlays and stack-vararg shortcuts. Real PCC and independent analysis consumers share these services. |
| `mfile1`, `mfile2`, `xdefs.c` | Explicit IR/resource/context contracts replace shared anonymous overlays. PCC parser symbol/scope/dimension state stays in task 193, outside the pass interface. |
| `allo.c`, `match.c`, `reader.c` | `allo.c`, `match.c`, `trees.c`, `reader.c` implement checked resources, template selection/expansion, tree encoding and streaming consumption. Physical ordering/spilling remains a target responsibility (194–196). |
| `cgram.y`, `scan.c`, `pftn.c`, `trees.c`, `optim.c` | Retained native working copies in `user/pcc` consume shared types/IR. Full native language/header closure remains 193. |
| `makefile`, `convert` | `tools/mip.mk` builds reusable native objects and private consumers. `user/pcc/output.c` selects direct or encoded consumption explicitly, replacing destructive one-pass conversion edits; full compiler build/install recipes remain 196. |
| `fort.c`, `fallo.c`, `fmatch.c`, `freader.c`, `fcomm2.c`, `flocal2.c`, `forder.c`, `ftable.c`, `fconvert` | Shared encoded streams/resources replace binary native-long overlays and source-include duplication. The independent producer checks this parser-free interface. Actual Fortran front end, machine target and driver are still owned by 196 and 203–206; no Fortran compiler is claimed by the interface test. |
| `g`, `INDEX` | Developer search/index aids, not shared runtime services; prototype/build-helper reconciliation remains 196. |
| lint consumers | The separate analysis executable links only `common.c`, `trees.c`, `reader.c`, consumes actual PCC passes and walks their checked IR. Full lint analysis passes/driver remain 261–262. |

## Acceptance evidence

`make TOOLCHAIN_PREFIX= test-mip test-mip-infra test-mip-pass test-pcc-front
 test-pcc-integer test-inventory` preserves and checks
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
execute and verify the result. This proves the shared template/pass pipeline.
PCC source acceptance additionally runs real retained parsing, code generation,
native assembly/linking and execution through both direct and split-pass modes,
with byte-identical assembly and independent GCC execution/ABI oracles. It covers
integer/pointer/control, scalar float/double, mixed GP/SSE calls, static
initializers and exact-size aggregate copies at 16/128 MiB as root and ordinary
user. This does not complete the remaining language, machine ABI or driver tasks.

The independent analysis consumer checks every generated PCC pass against a
Python framing/tree traversal oracle. The alternate producer includes full-width
constants/offsets, 255-byte names, floating nodes and aggregate metadata. Native
runs compare host/guest stream bytes and analysis results. Failure tests cover
truncation at every byte (including record boundaries), unknown versions/ABI/
reserved fields/kinds, bad widths/lengths, trailing bytes, embedded NUL, writer
limits, repeated completion, callback cancellation and failed output. The same
shared services run under host ASan/UBSan and native 16/128 MiB configurations.
Commit/test evidence is recorded in Git history.

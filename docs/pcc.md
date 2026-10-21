# Native PCC compiler implementation

Task 193 is **in progress**. `user/pcc` adapts the supplied mip grammar, scanner,
semantic tables, tree builder and optimizer to native LP64 and the checked shared
mip interface. The private front-end consumer runs after installed cpp and emits
semantic/IR inspection records. A separate private backend produces native
assembly and completes integer/pointer/control task 194. Scalar float/double and
call generation are implemented increments of task 195; aggregate, long-double
and variadic-definition ABI work remains. Installed command/alternate-target
integration remains task 196. Native header adapters and required extended language types are not yet
complete, so task 193 is not marked done.

## Implemented adaptations

- Retain and generate `cgram.y` with the V7 host yacc bootstrap and the checked
  native yacc parser skeleton. The grammar still reports seven shift/reduce
  conflicts. The source grammar/reference tree remains preserved separately.
- Use the shared native mip type word/operator definitions. Replace anonymous
  overlapping NODE structures with explicit fields and real native interfaces
  for pointer/wide returns and arguments. Short/int/long/pointer widths are
  16/32/64/64 bits; scalar/structure/union/array alignment follows native LP64.
- Keep complete identifiers through 255 bytes. Packed declarators use checked
  modifier insertion through thirty levels. Dimension, initializer, object-size,
  frame/alignment and scanner/line-marker limits fail with diagnostics.
- Give tags a separate lookup namespace and each structure/union its own member
  namespace. Resolve member references against the selected type, not the first
  matching global field spelling. Preserve member identities when symbol-table
  rehashing moves entries. Duplicate members are rejected.
- Parse native-width decimal/octal/hex integers and U/L suffixes. Fold integer
  operations through unsigned bit storage, with explicit signed comparisons/
  division, native width normalization and checked division/shift failures.
  Usual arithmetic conversions preserve signed long when combined with unsigned
  int; unsigned narrow arithmetic wraps to its declared width. Plain char is
  signed. `sizeof` produces native unsigned-long values.
- Preserve native pointer widths through indexing, element scaling and pointer
  differences. Integer-to-floating constant conversion honors unsigned values.
  Parameter names, scopes, declarations, initializers, labels, control flow,
  structure operations and literal data retain the supplied semantic machinery.
- Replace stack-varargs diagnostics with real native varargs. Syntax errors stop
  immediately rather than executing semantic actions on an invalid recovery
  stack. This changes diagnostic recovery, while retaining error status and
  avoiding invalid initializer-state access. Correctly parse initial cpp line
  markers before tokenization; reject oversized filenames and line overflow.

The mutable front-end pool currently permits 1024 symbols, 1024 expression nodes,
4096 dimensions, 128 initializer levels and the reference parameter/switch/control
limits. Individual size/frame tables retain checked signed-int bit offsets;
64-bit constant/type/size calculations are not narrowed silently. Large valid
objects may still exceed the frozen native loader's 8 MiB executable limit.

## Shared IR and source ownership

`ir.c` lowers retained front-end trees through an iterative postorder walk into
`user/mip`'s bounded arena. It carries exact names, storage class, byte offsets,
field descriptors, aggregate size/alignment and source line without pointer
serialization. The private consumer encodes each expression using the checked
mip pass format and inspects the resulting tree. Backend consumers will use the
same bridge; no separate test-only parser implements C semantics.

| Reference input | Working adaptation |
|---|---|
| mip `cgram.y`, `scan.c` | `user/pcc/cgram.y`, `scan.c`, native numeric token handling in `numbers.c`, generator `tools/pcc_generate.py` |
| mip `pftn.c`, `trees.c`, `optim.c`, `xdefs.c` | Retained working copies with native widths, conversion/bounds and member-namespace adaptations |
| mip `manifest`, `mfile1`, `common`, `comm1.c` | Shared `user/mip` contracts plus `manifest`, `mfile1`, `prototypes.h` and native mutable-tree/diagnostic adapter `common.c` |
| pcc `macdefs`, `local.c` | `macdefs` and `local.c`: native target sizes, pointer/scalar conversion and offset helpers; no PDP-11 name prefixes or short/int collapsing |
| pcc `code.c`, `local2.c`, `order.c`, `table.c`, `mac2defs` | `trace.c` enables semantic tests; `code.c`/`local2.c` emit native scalar code through shared mip templates; aggregate/variadic/extended floating work remains 195 |
| pcc makefile/prototype/fort targets | Native front-end rules in `tools/pcc.mk`; complete installed command and alternate consumers remain 196 |

## Acceptance evidence and remaining work

`make TOOLCHAIN_PREFIX= test-pcc-front` checks retained production parsing and
semantics under host ASan/UBSan. An independent GCC program verifies scalar,
structure, union, array, pointer and member-offset values plus conversion and
constant-fold results. Fixtures cover control flow, K&R parameters, aggregate
assignment/return IR, exact long names, maximum declarators, repeated field
spellings, tag namespaces, duplicate members, source bounds and deterministic
malformed-source mutations. At 16/128 MiB, root and ordinary users run installed
cpp into the native front end, compare exact semantic/shared-IR records, and
check error diagnostics. No machine-code acceptance is claimed by these tests.

Task 193 still needs the extended native type/header contracts required by ABI
version 1 and their backend integration. Aggregate, variadic-definition,
long-double and installed driver acceptance stay open in 193–196;
full supported-tool rebuild remains 207. The shared mip task also stays in
progress until its compiler/alternate consumer gates pass. Commit/test evidence
is recorded in Git history.

## Integer/pointer/control backend (task 194)

`code.c`, `local2.c` and `backend.h` implement the native replacement for the
PDP-11 code/local2/order/table target. The retained front end lowers real C
expressions into shared mip IR; ordered templates and the generic resource
allocator select checked integer operations. The initial target conservatively
spills live values, using RAX/RCX/RDX scratch registers and preserving callee-saved
registers. Frames and incoming GP/stack parameters follow SysV AMD64; frame size
tracks block high-water marks and stays 16-byte aligned. This is a private backend
consumer until task 196 installs the compiler command.

Integer generation covers native-width loads/stores/conversions, arithmetic,
signed/unsigned division/remainder, bit operations/shifts, comparisons, unary
operations, conditional/comma/short-circuit expressions, assignments and pre/post
updates. Compound lvalue addresses are evaluated once. Narrow compound operations
use promoted arithmetic types before narrowing; unsigned bitfield values promote
correctly, and stores preserve neighboring objects. Pointer indexing/differences
retain 64-bit intermediates. Global addresses use full-width relocations and
local address additions avoid host signed overflow.

Labels, if/loops, break/continue, goto and 64-bit switch cases emit native control
flow. Data hooks emit integer initializers, symbolic pointers, strings, zeroes,
commons and static locals in native sections without PDP-11 symbol prefixes or
runtime helpers. Floating values, aggregate copying/classification, call
instructions and variadic handling remain task 195; extended types/header and
public driver gates remain 193/196. Unsupported remaining operations fail rather
than emitting placeholder code.

`make TOOLCHAIN_PREFIX= test-pcc-integer test-pcc-front test-mip-infra
 test-inventory` checks production front-end/backend code under host sanitizers.
A separately compiled GCC caller and a GCC build of the same C functions verify
GP/overflow-stack arguments, signed/unsigned arithmetic, pointer/field stores,
byte-sized field boundaries, compound promotions, short-circuiting, loops,
switch/goto, static data and symbolic addresses above 4 GiB. Native 16/128 MiB
acceptance runs cpp -> private PCC -> installed as -> installed ld -> execution
as root and an ordinary user, comparing exact host/native assembly. It also
compiles a complete C main program and executes it with native crt0/libc.
Fixtures use ordinary separate translation units to keep assembler working sets
within the 16 MiB baseline; larger complete-tool rebuilds remain the 128 MiB
acceptance gate. This completes task 194, not the remaining compiler family.


Task 195 integer call increment implements direct and indirect calls through the
shared IR. Caller arguments use six GP registers followed by ordered stack
slots, with 16-byte call alignment even inside spilled expressions. Reserved
argument slots keep nested evaluations and the function pointer intact. Return
values are normalized to the declared native width. Integer ABI tests include
nine arguments, a separately compiled GCC callback, recursion, zero-argument
calls and nested calls. `make TOOLCHAIN_PREFIX= test-pcc-integer` passes host
ASan/UBSan/GCC oracles and native root/user 16/128 MiB execution. Floating,
aggregate and variadic call classification remain pending; task 195 is open.


Task 195 scalar floating increment adds SSE float/double loads, stores,
arithmetic, negation, comparisons (including unordered NaNs), truth tests,
compound assignments and conversions to/from native integer widths. Unsigned
64-bit conversions handle the range above INT64_MAX. Static floating data is
emitted as exact bit patterns. Calls and parameter entry independently allocate
six GP/eight SSE registers, then ordered overflow stack slots, and set AL to the
SSE argument count for calls into variadic callees. Default argument promotions
apply to unprototyped calls. The retained K&R parameter model is still in use;
ANSI prototypes and extended native types remain part of task 193.

`make TOOLCHAIN_PREFIX= test-pcc-integer test-pcc-front` verifies independent GCC
callers/callees, including a GCC variadic consumer, mixed GP/SSE register
exhaustion, nested floating calls, float rounding/stores, NaNs/signed zero and
unsigned conversion boundaries. Host sanitizer/oracle and native root/user
16/128 MiB gates run the same cases. Aggregate classification/copy/return,
long-double support and variadic function definitions remain open; this scalar
increment does not complete task 195.

# Native PCC front end

Task 193 is **in progress**. `user/pcc` adapts the supplied mip grammar, scanner,
semantic tables, tree builder and optimizer to native LP64 and the checked shared
mip interface. The private front-end consumer runs after installed cpp and emits
semantic/IR inspection records. It is not an installed C compiler or an assembly
producer. Integer/pointer/control backend work remains task 194, floating/call/
aggregate ABI work remains 195, and command/alternate-target integration remains
196. Native header adapters and required extended language types are not yet
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
| pcc `code.c`, `local2.c`, `order.c`, `table.c`, `mac2defs` | Private semantic target `trace.c` enables front-end tests; actual machine emission/ordering/calls remain 194–195 |
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
version 1 and their backend integration. Actual compiled-program, mixed-register,
variadic, long-double and installed driver acceptance stay open in 193–196;
full supported-tool rebuild remains 207. The shared mip task also stays in
progress until its compiler/alternate consumer gates pass. Commit/test evidence
is recorded in Git history.

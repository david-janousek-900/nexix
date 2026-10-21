# Native toolchain ABI version 1 (task 177)

The C toolchains target `x86_64-nexix`: ELF64 little-endian AMD64, LP64,
System V AMD64 base calling conventions and static executables. This contract
freezes the interface implemented by the current host bootstrap, native libc,
startup objects and kernel loader. Completing it does not complete either
native compiler, assembler or linker. Tasks 178–207 retain those owners.

## C representation and calls

| Type | Size | Alignment |
|---|---:|---:|
| char / signed char / unsigned char | 1 | 1 |
| short | 2 | 2 |
| int / enum | 4 | 4 |
| long / long long / pointer / size_t / ptrdiff_t | 8 | 8 |
| float | 4 | 4 |
| double | 8 | 8 |
| long double (x87 extended, including padding) | 16 | 16 |

Bytes have eight bits. Integers use two's complement and little-endian storage;
float/double are IEEE binary32/binary64. Plain char defaults to signed;
`-funsigned-char` is the existing V7-source build adaptation, not a different
object calling convention. Structures use System V natural layout, padding and
bit-field allocation; unions share offset zero. No implicit packed disk overlays
are allowed. Public historical disk formats retain their explicit encoders.

Integer/pointer arguments use RDI, RSI, RDX, RCX, R8 and R9, followed by stack
slots. SSE arguments use XMM0–XMM7; stack overflow follows the System V argument
classification and ordering. RSP is 16-byte aligned before call and is 8 modulo
16 on normal function entry. RBX, RBP and R12–R15 are callee-saved; the direction
flag is clear. User code is built without the red zone, AVX or AVX-dependent
types. There is no AVX calling-convention extension in version 1.

Integer results use RAX (and RDX for two INTEGER eightbytes); floating results
use the System V SSE or x87 result registers. Aggregate classification applies
to both arguments and results, including mixed INTEGER/SSE pairs. MEMORY-class
results use a caller-supplied hidden pointer; it is returned in RAX. Variadic
calls set AL to the number of SSE argument registers used, at most eight.
`va_list` uses the System V gp_offset/fp_offset/overflow_arg_area/reg_save_area
representation. Default argument promotions apply. Public `stdarg.h` exposes
the compiler's builtin implementation of that representation.

Compilers must support the syntax/builtins used by the native public headers,
including fixed-width integers, offsetof, static assertions, inline functions
and stdarg builtins, or supply an explicitly tested source adapter. Old PDP-11
pointer/int casts and stack-varargs conventions are not valid native shortcuts.
Task 79 documents replacement compiler runtime operations; historical helper
objects are never linked into native applications.

## Objects, executable layout and startup

Objects are ELFCLASS64/ELFDATA2LSB/EM_X86_64 ET_REL. Symbols have their C names
without an added underscore. They use ELF binding/type/visibility, string and
section tables, and explicit-addend SHT_RELA relocations. Required static
relocations are R_X86_64_64, PC32, PLT32, 32 and 32S; the linker must check
their signed/unsigned ranges, reject unresolved required symbols and reject
unsupported relocations. V7-source `-fcommon` symbols use SHN_COMMON until the
linker allocates them. Native archives use the SysV/GNU ar container; historical
archive reading and conversion remain tasks 179–180.

Executables are static ET_EXEC with ELF OSABI 0 and version 1. There is no
PT_INTERP, dynamic linking, PIE or TLS in version 1. `user/user.ld` is the
canonical application layout: text starts at 0x40000000, data starts on a
4096-byte boundary, text is RX and writable data/BSS are RW. The stack is
nonexecutable; BSS is zero-filled. The loader validates ELF headers, segment
bounds/permissions, the executable entry and its existing resource limits.
Linkers must preserve those loader checks rather than silently relax them.

At `_start`, the initial stack contains argc, argv pointers, a null, environment
pointers and a null. `crt0.o` derives those pointers, initializes environ, aligns
the stack and calls `main(argc, argv, envp)`; main's integer result goes through
exit/stdio cleanup. `fcrt0.o` has the same hardware-FP ABI. The two profiling
startup variants use the same calls and add the documented monitor lifecycle.
The library syscall wrapper translates System V arguments to Nexix's int 0x80
ABI; a compiler must not substitute Linux syscall numbers or instructions.

`ident.h` supplies the application-owned writable `myname` identity as V7 did,
plus target and ABI-version macros. Include it in one translation unit. The
historical `research 11/70` identity is not an x86_64 compiler target.

## Acceptance

`make test-toolchain-abi` builds separate native translation units with different
optimization levels and an independent assembly call inspector. Installed-header
guest tests run at 16/128 MiB, checking scalar layouts, mixed INTEGER/SSE and
MEMORY aggregate returns, argument-register exhaustion, stack alignment, AL for
variadic calls, va_copy, long double returns, argv/environment, static zero-fill
and native libc calls. Host inspection checks the emitted object/executable
machine, type, layout, permissions, symbols and relocation classes. Existing
`make test-development` covers all four startup objects and installed libraries.
Retargeted tools must run these same fixtures; host-bootstrap success alone
does not establish that a future native compiler is complete.

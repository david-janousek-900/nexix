# Native libc and installed development interfaces (task 82)

The full IDE distribution installs `/lib/libc.a`, `/lib/libm.a`, crt0.o,
fcrt0.o, mcrt0.o and fmcrt0.o, plus the complete current native public header
set under `/usr/include` (including `nexix/` shared ABI headers). Files are
root-owned mode 0644; directories are 0755. The fixed bootstrap stays small.
These are native ELF64 objects/archives; historical PDP-11 objects cannot link.
The GCC/binutils host bootstrap remains the compiler until tasks 177–207.

Normal and floating startup both initialize argc/argv/environ and invoke main,
then exit/stdio cleanup. Hardware FP is mandatory, so fcrt0 needs no simulator.
Profiling variants initialize native monitor and emit native mon.out on normal
exit. Every startup variant is compiled, staged and exercised by acceptance.

Native stddef/stdint/stdarg/limits headers provide the LP64 scalar and compiler
varargs contracts without relying on host system headers. stdarg uses the native
compiler's SysV builtins; retargeted compilers must implement the same calling
convention. The V7 private stack-based varargs header is replaced at source
callers, not installed as a misleading pointer-varargs shim. `whoami.h` supplies
the native system name Nexix. Existing header coverage lists every original
header and the public/private/deferred disposition.

`docs/libc-source-coverage.json` maps every one of the 174 libc-owned reference
inputs to active implementation, native compiler equivalent or build recipe.
`make test-libc-source-coverage` rejects omitted/duplicate inputs and missing
native paths. Public declaration/export checking, architecture/runtime/FP
acceptance and the existing libc/stdio/file/account/profiling suites provide
behavioral evidence; the mapping itself is not a numerical/conformance proof.
Original private FILE, remote printf, simulator and syscall representations
have the documented native adapters rather than binary-compatible overlays.

Task 82 also closes the remaining execvp consumer behavior: the original default
PATH begins with the current directory; colon and historical dash separators
are recognized; EACCES is retained across search; ENOEXEC invokes `/bin/sh`
with the candidate script and remaining arguments; ETXTBSY retries with the
source's 2/3/4/5-second sleeps. Native checked buffers reject overlong candidate
paths and script argument vectors with E2BIG instead of overwriting memory.
execl/execlp/execle keep their documented 64-argument native collection limit.

`make test-development` compiles a native consumer and adapted V7 cat **only
against the verified staged headers and libraries**, with `-nostdinc` and no
compiler/host header search path. It executes all four startup variants at
16/128 MiB, checks argc/argv/environment, mixed FP/variadic calls, buffered FILE
I/O and exit cleanup, parses profiling output, and checks script PATH fallback
through the real shell. `make test-exec-search-unit` injects errors and validates
busy retries, script arguments, permission precedence and overflow bounds.

This completes the native libc implementation and current development ABI.
Remaining full application ports still require their own consumer acceptance;
subsequent discovered integration defects must be fixed in those tasks. Seven
original application-owned format headers (archive, dump/old dump, compiler
identity, multiprecision, standalone I/O and tp) remain explicitly assigned to
179/232/234/177/109/344/236. They do not belong to the libc ABI and are not
installed as unsupported native record overlays. Their future tasks also own
the remaining distribution.headers integration status.

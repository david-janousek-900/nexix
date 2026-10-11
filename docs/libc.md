# Native libc foundation (step 11)

Nexix builds `build/crt0.o` and `build/libc.a` for statically linked ELF64
programs. They are native AMD64 implementations using the Nexix syscall ABI.
Historical PDP-11 assembly is retained under reference/ and is not linked.
The supported interface is a foundation for rebuilding V7 userland, not a
complete libc. File I/O, signal wrappers and brk/sbrk now have kernel support.
Step 16 adds integer stdio, heap allocation and V7 source compatibility; see [userland.md](userland.md).

## Build and startup

```
make TOOLCHAIN_PREFIX= userland
make TOOLCHAIN_PREFIX= test-libc
```

The first command builds the startup object, archive, and `build/libc-test.elf`.
The second boots the complete tests in QEMU with 16 MiB RAM and saves
`build/libc-16M.log`. Normal `make TOOLCHAIN_PREFIX= smoke` runs them with
128 MiB. `make -j3 TOOLCHAIN_PREFIX= test` includes all regression groups.

User C compilation is freestanding with `-nostdinc`, using only Nexix headers
and the compiler's freestanding headers (stddef/stdint and similar). It does
not use host OS headers or host libc. Linking uses ld with `-nostdlib`, the
startup object first, application objects next, and the archive last. The
shared `user/user.ld` places programs at USER_BASE with separate RX and RW/NX
segments and a non-executable stack declaration. For example:

```
ld -nostdlib -z max-page-size=0x1000 -T user/user.ld \
   build/crt0.o program.o build/libc.a -o program.elf
```

Compile application objects with the Makefile's USER_CFLAGS and the same
compiler used for libc. The target supports x87/MMX/SSE/SSE2, with AVX disabled,
no red zone, no PIE, no stack protector or unwind tables. Only kernel C retains
`-mgeneral-regs-only`. TLS, shared libraries and constructors remain deferred.
A generated ELF requires Nexix and cannot run using the host's syscall ABI.

`_start` reads the step 10 stack, initializes `environ`, clears DF, clears the
frame pointer and calls `main(argc, argv, envp)` using the SysV AMD64 calling
convention and stack alignment. A conventional two-argument main also works;
the third argument is available to programs which use it. Returning from main
calls `exit` with its return value. Empty argument/environment vectors and the
loader's maximum 64 entries per vector are supported. See [exec.md](exec.md)
for the initial stack layout and argument limits.

## Interfaces

Headers live under `libc/include/`; native kernel ABI definitions remain under
`include/nexix/`. Public PID, UID/GID, mode, offset and clock types match the
native ABI. `wait` writes a four-byte int, and `struct tms` contains four
signed 64-bit tick counts. CLK_TCK is 100.

Implemented wrappers:

- `_exit`, fork, wait, getpid, getppid.
- getuid, geteuid, getgid, getegid, umask, times, sched_yield.
- execve and execv. Following V7's libc behavior, execv calls execve with
  `environ`; the raw kernel exec syscall 11 instead uses an empty environment.

The assembly `nx_syscall6` entry converts seven SysV C arguments (number and
six full-width values) into RAX/RDI/RSI/RDX/R10/R8/R9 for `int 0x80`. It returns
the raw signed result or negative errno and preserves callee-saved registers.
Its declaration is in `<nexix/libc.h>`; raw calls do not change libc errno.
The C wrappers translate errors to -1 and set positive `errno`. Success leaves
errno unchanged. `errno` is a global in each process's private VM; threads,
TLS-based errno is deferred. Signal handlers must preserve errno themselves
when needed. NX errors retain
V7 numbers, including native ENOSYS=35, independently of host errno values.

Also implemented:

- memcpy, memmove, memset, memcmp.
- strlen, strcmp, strncmp, strcpy, strncpy, strchr, strrchr.
- getenv, reading the current `environ` vector.
- exit and atexit, with 32 statically stored callbacks executed in reverse
  registration order. `_exit` bypasses callbacks. Step 36 flushes buffered output after callbacks; successful exec replaces the callback and stream state with fresh BSS.

Step 13 adds file and pipe wrappers and headers; see [files.md](files.md).
Step 14 adds sgtty/ioctl headers and stty/gtty/ioctl/isatty; see [tty.md](tty.md).
Step 15 adds signals, brk/sbrk, setjmp/longjmp and init/shell service wrappers;
see [signals.md](signals.md). Step 16 adds malloc and userland support; see
[userland.md](userland.md). String
functions follow their usual C buffer contracts; they do not validate pointers
or allocate storage. getenv returns a pointer into the environment, not a copy.
The wait-status helpers expose exited and signaled status encodings, including
processes terminated by native signal delivery.

## Validation

The real C fixture at `/bin/libc-test` is loaded and run at CPL3 using crt0 and
the archive. It checks arguments/environ, empty and full vectors, stack
alignment, zeroed BSS, six full-width syscall arguments and callee-saved
register preservation. A bootstrap-only int 0x81 helper inspects the arguments
saved by the real int 0x80 call; libc itself uses only the production gate.

Further tests cover raw/translated errors, successful-call errno retention,
identities, tick accounting, umask, yielding, overlapping memory moves,
unsigned byte comparisons, string boundaries and getenv lookup. Fork checks
private errno/data, followed by execve with a changed environment and execv
with the retained environment. Returning from main exercises exit callbacks
and ordinary exit statuses. A separate child fills the callback table and
verifies `_exit` bypasses it. Each test run recovers all VM pages and process
slots. A failure status identifies the C fixture's failing CHECK line when
that process exits directly.

Every successful boot requires `NEXIX_LIBC_ABI_OK` and `NEXIX_LIBC_OK` before
`NEXIX_BOOT_OK`. These libc fixtures alone do not prove a userland port; the rebuilt V7 init,
shell and utilities have separate acceptance tests (see [userland.md](userland.md)).
Step 12 implements the block interface, RAM root device and V7 filesystem
foundation; see [filesystem.md](filesystem.md). Steps 13 and 14 add file
operations, pipes and terminal controls; see [files.md](files.md) and [tty.md](tty.md).

Step 28 adds binary64 `frexp`, `ldexp`, `modf`, `fmod`, decimal `atof`/`strtod`,
`ecvt`/`fcvt`, and `%f`, `%e`, `%E`, `%g`, `%G` formatting. See
[floating-point.md](floating-point.md) for numeric contracts and limitations.
`build/libm.a` contains native adaptations of all 16 retained libm C files;
link math callers with `build/libm.a build/libc.a` after their objects.

Steps 33–35 add scanf/fscanf/sscanf and their va_list variants, stream pushback,
fdopen/freopen, gets/getw/putw, V7 character classification and string aliases,
native integer conversions and the V7 rand sequence. See
[libc-coverage.md](libc-coverage.md) for behavior and remaining compatibility work.

Step 36 (previously 25.8) adds lazy 512-byte stream buffering and working V7 setbuf. Terminal
output and initial stderr remain unbuffered. fflush/fclose and normal exit
flush pending output; _exit and successful exec do not. Input fflush is a V7
no-op. See [libc-coverage.md](libc-coverage.md) for buffer ownership, seek/tell,
update-stream and fork conventions. All native callers must rebuild because
FILE now includes buffer state.

Step 40 adds [V7 scalar/byte/conversion helpers](libc-helpers.md), including
returning abort, historical disk-address packing, gcvt, tell, execle and
exit-based assertions. Public declaration/export coverage is checked separately
from complete feature behavior; libc remains partial.

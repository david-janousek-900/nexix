# V6 wrappers and compiler runtime equivalents (task 79)

Nexix's supported application ABI is native x86_64 SysV LP64 and the Nexix
syscall ABI. Reference V6 files are adapters for running V7 applications on a
V6 kernel; they are not an additional Nexix kernel ABI. Native applications use
the same public function names with native records and argument widths. V6
16-bit binaries, executable formats and self-modifying syscall stubs are not
supported. The reference tree is preserved.

| V6 reference input under `usr/src/libc/v6` | Native replacement |
| --- | --- |
| `access.c` | `libc/syscalls.c` access; real native permission checks replace the V6 stat-only approximation |
| `chown.c` | `libc/syscalls.c` chown; separate UID/GID arguments replace packed V6 byte IDs |
| `execl.c` | `libc/exec.c` execl with native va_list and pointer vector |
| `ftime.c` | `libc/time.c` ftime reads the native clock; the V6 wrapper's fixed dummy date is unnecessary |
| `gtty.c` | `libc/syscalls.c` gtty and native sgtty record |
| `lseek.c` | `libc/syscalls.c` lseek takes the native byte offset directly; no V6 block-seek split |
| `stat.c` | `libc/syscalls.c` stat/fstat with native stat record; no V6 record translation |
| `syscall.s` | `libc/arch/x86_64/syscall.S` fixed native argument registers and int 0x80 |
| `time.s` | Native time through `libc/syscalls.c`; native time_t rather than a V6 register pair |

| PDP-11 reference input under `usr/src/libc/crt` | Native replacement |
| --- | --- |
| `aldiv.s`, `almul.s`, `alrem.s` | Native C compound division, multiplication and remainder assignments |
| `ldiv.s`, `lmul.s`, `lrem.s` | Compiler-emitted x86_64 signed division/multiplication/remainder |
| `csv.s` | Compiler-emitted SysV function prologue/epilogue and callee-saved registers replace csv/cret |
| `cerror.s` | `libc/syscalls.c` converts negative syscall returns to -1 and errno |
| `mcount.s` | `libc/arch/x86_64/mcount.S`, monitor/profil; task 60 |

These historical helper symbols are internal to the PDP-11 compiler calling
convention. Defining native functions named csv/cret or aldiv would not make
that calling convention valid. Native compiler retargeting in tasks 177–207
must emit the native operations/ABI; it must not link historical object files.
The current GCC bootstrap emits no unresolved helper imports for the arithmetic
fixture. Startup/crt0 variants, installed headers and consumer closure are task
82. Floating simulator/fakfp replacement is task 81.

`make test-runtime-equivalent` runs native compound and signed arithmetic,
recursive stack frames, register/stack variadic arguments, real ftime, stat,
access/chown and errno tests in private QEMU roots at 16 and 128 MiB. Existing
libc/file/TTY suites cover native exec, seek, time and gtty; no V6 layouts are
used by these programs.

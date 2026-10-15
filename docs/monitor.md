# Monitor, mcount and profiling startup (task 60)

Nexix supplies `monitor`, `mcount`, `countbase`, `build/mcrt0.o` and
`build/fmcrt0.o`. They use [task 59's profil sampling](profiling.md) and emit
a native `mon.out`. The prof reader and native compiler drivers remain later
tasks; this does not install a complete development toolchain.

The V7 sources are `libc/gen/mon.c`, `libc/crt/mcount.s`,
`libc/csu/{mcrt0,fmcrt0}.s` and `monitor(3)`. The reference cc/C/Fortran
compiler paths select these startup objects and emit instrumentation; native
retargeting remains their respective tasks. Supplied reference files are unchanged.

## Manual interface

Include `<mon.h>`. A start takes a low/high PC range, a live writable unsigned
short buffer, its length in shorts, and call-record capacity:

```
monitor(low, high, buffer, sizeof(buffer)/sizeof(buffer[0]), (size_t)300);
/* ... instrumented calls and sampled execution ... */
monitor(NULL);
```

The declaration is variadic to retain V7's one-argument `monitor(0)` stop.
For starts, high must have type void pointer, buffer unsigned-short pointer,
and both counts native `size_t`; cast integer constants accordingly. Functions
used as bounds should be explicitly converted to void pointers. Start/stop
return zero or -1/errno, making errors observable through the native interface.

Start validates sizes/range and two-byte buffer alignment before altering an
existing session. It stops previous sampling, initializes the native header and
call table, and preserves caller-provided histogram values, as V7 mon.c did.
Zero the buffer for a fresh histogram. Capacity zero permits sampling only;
no histogram space permits call counting only. Insufficient header/table space,
reversed ranges, pointer/size overflow and capacity above UINT32_MAX are EINVAL.
The caller must retain the buffer until it replaces/stops the session and until
any requested output/retry completes. Normal libc pointer-validity rules apply.

The scale follows V7 monitor: compare histogram shorts with half the text span,
otherwise calculate floor(histogram-shorts * 32768 / text-words), then double.
The native implementation avoids multiply overflow and external division helpers.
Sampling uses the full remaining histogram storage, with V7's word granularity;
the high PC defines scaling and call-record bounds. Scale can become zero for
very large spans; calls can still be counted. Manual monitor use requires an
explicit stop/output unless linked with a profiling startup object.

Stop disables kernel sampling and call recording before creating `mon.out`
with mode 0666, subject to umask, in the current directory. It writes the entire
buffer, handles short writes/EINTR, closes the descriptor and preserves the
first output error. A later stop can retry the same snapshot. Stop before start
only disables sampling; it creates no empty profile. Replacing a session does
not automatically write the previous one.

## Call instrumentation and state preservation

The native mcount entry accepts a plain compiler-inserted call and uses its
return PC as the instrumented function's identifier, as V7 records the return
PC of its entry hook. GCC x86_64 `-pg` works with the Nexix freestanding flags;
there is no dependency on host gmon runtime or a Linux libc. Future native V7
compilers must emit this native call convention rather than PDP-11 R0/site slots.

The assembler preserves every general register, flags and FXSAVE64 state
(x87/MMX/XMM registers and controls), with an aligned temporary stack frame.
This preserves incoming integer, stack, floating-point and variadic arguments.
The C update helper is compiled without instrumentation and does no allocation,
syscalls or errno changes. A guard skips nested signal-handler counting while
an update is in progress; reentrant monitor operations return EBUSY instead of
blocking. There are no threads in the current process model.

Each distinct in-range PC receives one 16-byte call record, with a wrapping
64-bit count. Lookup is bounded by capacity. Once full, unseen PCs are skipped
and existing records continue counting; the histogram is never used as overflow
call storage. `countbase` exports the next available record address or NULL
when full/inactive. Setting it to NULL prevents new allocations but preserves
existing counts, like V7's exhausted allocation cursor. Record storage is
controlled by the session, not by arbitrary externally supplied cursor values.

## Startup and exit

Link a profiled program with `build/mcrt0.o` instead of `build/crt0.o`, plus
`build/libc.a` and `user/user.ld`. Compile the functions to instrument with `-pg`
in addition to Nexix's ordinary user flags. `build/fmcrt0.o` uses the same native
hardware-FP path. Nexix requires hardware FP/SSE2; it does not install the
PDP-11 fmcrt0 illegal-instruction simulator. Both variants are exercised with
integer, variadic, double, x87 and SSE state tests.

Startup preserves argc/argv/envp, initializes environ, allocates a zeroed buffer
with 300 call slots and approximately one histogram short per eight text bytes,
and starts monitor before main. Linker PROVIDE symbols expose the text end
only when referenced, preserving ordinary startup and executable layouts.

A profile finalizer is registered before main's atexit handlers. Normal return
from main or exit runs user callbacks first, flushes stdio, stops profiling and
writes mon.out. The finalizer ignores output failure when preserving the exit
status, as the historical startup did. Explicit monitor calls report errors.
Allocation/setup failure prints `No space for monitor buffer` and exits 127;
main is not entered. Ordinary programs retain their original startup/exit path.

`_exit`, successful exec and fatal default signals bypass the finalizer, so no
profile is emitted. Fork inherits the private buffer/session like other libc
state; children that also exit normally write the same CWD pathname and may
replace their parent's output, as in V7. The acceptance child uses `_exit` to
verify independent counters without overwriting the parent's file.

## Native file format, version 1

All fields are little-endian. The 64-byte header is described by
`include/nexix/mon.h` and `<mon.h>`; decode bytes or memcpy into aligned structs.
The buffer itself requires only two-byte alignment.

| Offset | Field | Width/value |
|---:|---|---|
| 0 | magic | Eight bytes: NXMON followed by three zero bytes |
| 8 | version | uint32, 1 |
| 12 | machine | uint16, ELF EM_X86_64 (62) |
| 14 | header_size | uint16, 64 |
| 16, 24 | low_pc, high_pc | uint64 each |
| 32, 36 | tick_hz, scale | uint32 each; 100 Hz and configured sampling scale |
| 40, 44 | call_capacity, call_used | uint32 each |
| 48 | histogram_bytes | uint64 |
| 56 | call_record_size | uint32, 16 |
| 60 | counter_size | uint16, 2 |
| 62 | reserved | uint16, zero |

The header is followed by `call_capacity` records `{uint64 pc, uint64 count}`,
then `histogram_bytes` bytes of uint16 samples. Used records are packed first;
unused call slots are zero. File length is exactly 64 + 16*capacity + histogram
bytes. This is a native record format, not a PDP-11 mon.out overlay or GNU gmon
file. Task 257's prof port must decode this version and use its recorded tick rate.

## Acceptance

`make test-monitor` runs host error/format/scale/counting fixtures and QEMU
acceptance at 16/128 MiB, plus inventory/header/export checks. Host tests cover
full/zero tables, histogram preservation, 64-bit wrap, high PCs, two-byte buffer
alignment, start/stop/retry, partial/EINTR/failed/zero writes, close failures,
reentrancy and allocation/exit-registration failures.

Native tests use real `-pg` programs and both startup objects. They verify exact
call-count distributions, recursion, real histogram ticks, all general registers,
flags, x87/SSE controls/data, integer/stack/double/variadic arguments, argc/argv/env,
normal return/exit/atexit flushing, fork isolation, `_exit` and exec bypass,
manual counting/sampling and the independent binary layout/length parser.
The probe installer is private to root-exec tests; nothing is installed in the
normal root image. `NEXIX_MONITOR_OK` is required before existing cleanup/boot
markers. Host units, shell and cold-boot persistence provide regression coverage.

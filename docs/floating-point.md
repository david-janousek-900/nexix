# Native floating-point support (step 25, in progress)

Nexix initializes x87, MMX and SSE/SSE2 before enabling the timer. The CPU must
advertise x87, FXSAVE, SSE and SSE2. CR0.EM/TS are cleared, MP/NE enabled, and
CR4.OSFXSR/OSXMMEXCPT enabled. OSXSAVE stays disabled: AVX and later extended
register state are not supported. Kernel C still uses integer registers only.

Each process has an aligned 512-byte FXSAVE64 image. The scheduler eagerly saves
and restores it on every process switch. New processes start with zero register
contents, an empty x87 stack, control word 0x037f and MXCSR 0x1f80. Fork copies
the live parent's FP state; successful exec resets it. Failed exec retains it.
FXSAVE's reserved bytes are cleared before saving, so signal frames cannot
expose uninitialized kernel stack bytes.

ABI version 2 extends the user signal context from 176 to 688 bytes. Delivery
saves FP state after the integer frame; sigreturn restores it after validating
both frames. The CPU's MXCSR_MASK limits accepted MXCSR bits, with Intel's
0xffbf fallback when the mask is zero. A forged reserved bit returns EINVAL
instead of causing a kernel fault. Nested signals naturally preserve each
interrupted handler's state. Handler longjmp remains possible because contexts
live in user memory. The native nine-word jmp_buf additionally restores x87
control and MXCSR; rebuilding callers is required when changing libc headers.

Implementation follows the [Intel architecture manual](https://www.intel.com/content/dam/support/us/en/documents/processors/pentium4/sb/25366521.pdf)
FXSAVE/FXRSTOR state format and MXCSR mask guidance.

## Native libraries

`libm/v7/` adapts all 16 C sources retained in `reference/v7/usr/src/libm/`.
The original files remain untouched. Changes add native declarations, share
libc errno, replace unsafe native integer casts used for tangent quadrants and
negative-base power parity, and handle IEEE nonfinite values where a historical
algorithm would loop or convert them to integers. `v7_cabs(struct v7_complex)`
is the historical two-field structure interface, not C99 complex arithmetic.

V7 polynomial coefficients, finite-input algorithms and domain rules are
retained. For example sqrt(-1) returns zero with EDOM and log(0) returns -HUGE
with EDOM; HUGE retains the V7 value. This is not a claim of C99 libm conformance
or exhaustive correctly-rounded accuracy. The historical header's gamma is
not implemented; this archive has no gamma implementation C file. Installing
the archive and native headers on the root filesystem is still pending.

Libc supplies IEEE binary64 frexp, ldexp and modf replacements for the PDP-11
assembly helpers, and an x87 FPREM implementation of fmod. Decimal atof/strtod
accept whitespace, signs, fractions and decimal exponents, report ERANGE on
overflow/underflow, and preserve the first unconsumed character. Hexadecimal
input and textual inf/nan are not supported. These conversions are not promised
to be correctly rounded for every decimal input.

ecvt/fcvt use a shared buffer with up to 64 requested digits, enough backing
storage for the binary64 exponent range, and historical half-up rounding.
printf-family floating formats accept precisions through 63 (default 6), mixed
integer/FP arguments and existing width/zero/left padding. Higher precision
returns EINVAL. The existing formatter still lacks some flags and conversions;
full V7 stdio coverage is unfinished.

## Validation

Run `make TOOLCHAIN_PREFIX= test-fpu`. It boots the complete self-test image
with 16 MiB and 128 MiB, covering live x87/XMM0/XMM15 register isolation across
yield and timer preemption, fork inheritance, exec control reset, signal handler
clobber/restoration, forged MXCSR rejection, and setjmp/longjmp controls. Native
C tests exercise all retained libm function families against fixed numeric
values, subnormals, signed fractions, decimal range errors, rounding, mixed
variadic FP arguments and truncated output buffers. All existing process and
filesystem cleanup checks still run.

The local QEMU 10.0.13 TCG records an unmasked SSE invalid-operation flag but
does not deliver #XM in the tested case. Acceptance distinguishes that result
from missing flag behavior; an emulator that delivers #XM must terminate the
child with SIGFPE. This test therefore does not prove actual SSE trap delivery
on this TCG build. x87 FWAIT exception delivery is tested separately. Kernel
vectors 16 and 19 both map to SIGFPE.

## Remaining step 25 work

This work does not complete step 25. Remaining prerequisites include broader
libc/stdio and native header coverage, account database and crypt support,
mount/raw-device administration,
process accounting, tracing/profiling and a native kernel-information interface
for ps/pstat/iostat. Historical phys/lock/mpx and special-device requirements
also need an explicit compatibility decision; returning ENOSYS is not a port.
Full program integration and native compiler work remain step 28.

The same step now supplies mknod, privileged V7 directory link/unlink operations
and per-process chroot; see [files.md](files.md). Their interfaces are tested
separately from arithmetic and FP register state.

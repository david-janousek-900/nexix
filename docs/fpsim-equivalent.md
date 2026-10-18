# Native floating-point simulator replacement (task 81)

V7 `usr/src/libfpsim` decodes and simulates PDP-11 FP11 instructions when that
hardware is absent. Nexix builds x86_64 ELF programs and requires x87/SSE2, so
native hardware instructions replace this instruction interpreter. This is an
architecture port, not execution support for PDP-11 binaries or their floating
representations. Native double is IEEE binary64, not the PDP-11 D format.

| Reference input | Responsibility | Native implementation |
| --- | --- | --- |
| `fp1.s` | Trap entry, instruction decode, addressing, FP mode/status | Native compiler instruction selection; `kernel/arch/x86_64/fpu.c` initializes controls and saves/restores process state |
| `fp2.s` | Floating loads/stores, sign, comparison and conversion operations | x87/SSE2 instructions emitted for native C; `libc/float.c` decomposition/remainder helpers |
| `fp3.s` | Add/subtract/multiply/divide and normalization support | Native hardware arithmetic/comparisons/conversions; process and signal FP contexts |
| `fpx.s` | Simulator state and shared temporaries | Per-process aligned FXSAVE64 state, native stack/register ABI |
| `compall` | Assemble simulator objects | Makefile builds native FP initialization/runtime and native programs |
| `mklib` | Insert simulator in libc | Makefile builds native libc.a; no interpreter object is required |
| `libc/gen/fakfp.s` | Restore default illegal-instruction disposition in lieu of simulator | Native invalid-opcode exception delivers SIGILL; missing required CPU FP capabilities prevent boot |

The historical object `fpsim.o` and its build scripts are fulfilled by these
native equivalents, rather than installed as unusable PDP-11 objects. There is
no libfpsim.a in the reference build. Kernel FP exception vectors map to SIGFPE;
unsupported instructions map to SIGILL. See [floating-point.md](floating-point.md)
for control registers, process/signal isolation and QEMU exception limitations.

`make test-fpsim-equivalent` boots private roots at 16 and 128 MiB. It checks
arithmetic, comparisons, sign, single/double and signed-integer conversions,
frexp/ldexp/modf/fmod/sqrt and actual invalid-instruction termination. `make
test-fpu` additionally verifies register/control isolation across scheduling,
fork, exec and signals. No PDP-11 binary-compatibility claim follows from either.

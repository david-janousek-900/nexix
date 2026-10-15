# V7 profil sampling (task 59)

Nexix implements syscall 44 and a libc `profil` wrapper. The V7 sources are
`sys/sys4.c` (`profil`), `sys/clock.c` (user-tick sampling), `sys/trap.c`
(system-time charging), `conf/mch.s` and `40/m40.s` (`addupc`), `h/user.h`
(`u_prof`), `sys/sys1.c` (exec reset), `sys/sysent.c`, libc `sys/profil.s`
and `profil(2)`. The reference files remain unchanged.

Include `<unistd.h>`:

```
int profil(unsigned short *buffer, size_t bytes, uintptr_t offset,
           unsigned int scale);
```

Addresses, byte lengths and PC offsets are native 64-bit values. Counters are
16-bit little-endian words; additions wrap modulo 65536, including multi-tick
system charges. Scale is V7's unsigned 16-bit fixed-point fraction. Values above
65535 return EINVAL without changing the current configuration. Valid calls
return zero; libc reports errors through -1/errno. No privilege is required.

## Bucket selection and bounds

The original `addupc` shifts PC-minus-offset and scale before multiplying, then
rounds its byte result up to an even word index. Nexix preserves that operation
order, rather than substituting a different modern profil formula:

```
delta = pc - offset
index = (((((delta >> 1) * (scale >> 1)) >> 14) + 1) & ~1)
```

A PC below offset is ignored. Multiplication uses a 128-bit intermediate to
avoid overflow with native PC values; the result must select a full two-byte
counter within the declared byte length. Out-of-range samples do nothing.
The base-plus-index addition is checked independently before any user access.
The complete selected word must lie in mapped, writable user memory and be
aligned to two bytes. Kernel addresses, missing/read-only pages, odd addresses
and address overflow disable profiling by clearing the stored scale. They do
not deliver a signal, corrupt a partial counter or change syscall/user errno.
Only the selected counter is checked, not the entire declared buffer.

Configuration stores the parameters without touching the buffer. Scale 0 or 1
is off, as documented by V7 `profil(2)`; size zero is ineffective without
changing the stored scale. Zero-tick charges touch nothing. An invalid buffer
therefore faults only when a nonzero, in-range charge reaches it. A later valid
call can re-enable profiling after a fault.

Two native adaptations make bounds explicit: PC subtraction does not wrap at
16 bits, and odd buffer lengths never authorize a word beyond their last byte.
There is also a supplied-source discrepancy: scale 1 becomes a zero multiplier
in the PDP-11 assembler and would increment bucket zero despite the manual's
off rule. Nexix follows the documented off behavior for scales 0 and 1.

## Clock, traps and process lifetime

`proc_timer` samples the interrupted user RIP once per user-mode PIT tick,
before a possible context switch. Nexix uses its existing 100 Hz clock rather
than V7's 60 Hz; counter units agree with `NX_CLK_TCK`, `times` and accounting.
Kernel PCs are never passed to the user profiler.

On return from a user exception, syscall or active private test gate, the
interrupt path charges the difference in that process's own `u_stime` to the
final user RIP, matching V7 `trap.c`. Signal delivery and tracing may have
changed that RIP. Sleeping, scheduler idle time and another process's CPU time
are excluded. Ordinary production syscall handlers currently keep IRQs disabled,
so many calls accumulate zero system ticks; the profiler does not invent time
from global elapsed ticks. There is no direct sample of kernel-mode IRQ PCs.

Fork copies the profiling parameters with the user area and the counter bytes
with the eager private VM. Both processes remain enabled, with independent
counter updates. A successful exec clears the scale before entering its new
image; failed exec preserves the old configuration. Existing VM replacement,
heap shrinking and exit release the memory normally: profiling pins no pages,
allocates no kernel buffers and holds no filesystem references. Removing a
buffer mapping disables profiling at the next attempted update.

[Task 60](monitor.md) implements mcount, monitor, profiling startup objects and
native profile file records. The prof command and symbol interpretation remain later consumer
tasks. Syscall 44 provides sampling into memory; it does not emit mon.out or
profile the kernel itself. The public ABI version and existing records are
unchanged by this additive syscall implementation.

## Acceptance

Run `make test-profile`. Host golden cases independently specify V7 bucket
indices, native address arithmetic, full-word bounds, overflow, wrapping,
scale/size modes, configuration rollback and quiet read/write faults.
Disposable-root QEMU runs at 16/128 MiB verify:

- Native high-stack pointers/64-bit lengths, bucket/counter results, invalid-scale
  rejection and preserved configuration.
- Lazy faults for odd, supervisor, missing, overflowed and RX buffers; no
  code changes or signal/errno side effects, including after heap shrink.
- Fork inheritance with private counters, failed-exec preservation and
  successful-exec reset in a real filesystem executable.
- Real user-mode PIT samples as root and an unprivileged user, actual kernel-residence ticks charged on trap
  return, and no system-time increase while the process sleeps.

The private fixture hooks inspect scale, inject deterministic samples, spend
three ticks in an IRQ-enabled kernel context and sleep for three ticks. They
operate only during root-exec self-tests; sample/time injections require root.
The ordinary sampling path uses actual timer interrupts and saved user PCs.
The required marker is `NEXIX_PROFILE_OK`, followed by the existing process,
file/TTY/page/filesystem cleanup and boot markers. The test target also runs
inventory, isolated header/ABI and libc export checks. Host unit, shell and
cold-boot persistence tests provide regression coverage; the working IDE disk
is preserved.

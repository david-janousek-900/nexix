# Debugger memory writes and single-step (task 58)

Task 58 completes the ptrace memory/stepping requests on top of
[task 57's stop/wait/register interface](ptrace.md). It does not install adb;
its ELF/core readers and live debugger remain tasks 258–260.

The V7 sources are `sys/sig.c` (`procxmt` requests 4, 5, 9 and shared continue),
`sys/trap.c` (trace/breakpoint signal and clearing TBIT), `sys/ureg.c`
(`estabur`/`sureg` protections), `conf/mch.s` (`suiword`/`suword`),
`sys/text.c`, `h/{text,reg}.h`, `sys/machdep.c` (`sendsig` trace flag handling),
libc `sys/ptrace.s` and `ptrace(2)`. `adb/runpcs.c`, `access.c` and `defs.h`
are the reference consumers for breakpoint insertion/restoration and stepping.
The reference files remain unchanged.

## Memory changes

Requests use aligned eight-byte words, matching task 57's native reads.
All positive requests still require the caller's stopped direct child; there
is no root bypass or arbitrary attach. Bad child addresses, alignment, mapping
permissions or executable ownership return EIO without modifying the word.

| Request | Native behavior |
|---|---|
| 4 (`NX_PT_WRITE_I`) | Write through a kernel physical alias to a user page that is executable or writable. Require exclusive, non-sticky executable-text ownership when the child has a text entry. |
| 5 (`NX_PT_WRITE_D`) | Write only through the ordinary writable-user-memory copy path. Read-only code remains protected, even for an exclusive executable. |

Instruction/data spaces coincide in the native VM, so request 4 can also
address writable data; V7's executable ownership checks apply to that request
regardless of destination. Shared text (`x_count != 1`) and sticky executables
are rejected for request 4, even though Nexix uses eagerly copied private pages.
Request 5 can modify a child's private data while its executable is shared.
Embedded bootstrap images without an inode/text entry use the same mapping
checks without an executable inode check.

`copyout_instruction` validates the full user range and every page before
storing any bytes. It rejects supervisor, absent and read-only non-executable
pages. The physical alias avoids changing page permissions: executable leaves
remain RX and data leaves remain NX. IRQ serialization prevents a mapping
change during access; the single-CPU return/context switch supplies the native
instruction synchronization. No user-writable executable mapping is created.

Successful request 4 marks the owner XWRIT. Modified bytes stay in the child's
VM and are copied by eager fork; fresh exec loads the original file. Replacing
or exiting the image releases its private pages through the existing VM/text
lifecycle. No swap image or write-back of debugger changes exists. Unlike the
PDP-11 path's clearing of ITEXT, Nexix retains that inode flag: ordinary file
writes, truncation and final unlink must continue respecting the active
executable. The existing ETXTBSY lifetime and sticky eviction rules remain.

## Stepping and breakpoints

Request 9 (`NX_PT_STEP`) validates address/signal exactly as continue, then
sets x86_64 RFLAGS.TF and resumes. Address 1 preserves RIP; another address must
be executable user memory. Data replaces the pending signal set, with zero
suppressing it. Invalid requests leave PC/flags and stopped state intact.
Request 6 may also write TF while still masking privileged/reserved flags.

A completed user instruction raises #DB, which becomes SIGTRAP and clears TF
before traced stop/wait handling, as V7 clears TBIT. Ordinary continue after
that stop does not keep stepping. INT3 is available at DPL3; #BP likewise
becomes SIGTRAP, clears TF and leaves RIP after the one-byte breakpoint.
A debugger restores the saved word, rewinds RIP to the patched instruction,
steps it, and may reinsert the breakpoint. No hardware-breakpoint-register
interface is added. Untraced caught/ignored debug traps resume normally;
ignored faults that would repeat the same instruction retain the fatal policy.

INT 0x80 clears hardware TF on kernel entry. When TF was set on syscall entry,
the native interrupt path posts SIGTRAP at completion before another user
instruction, with TF cleared. Kernel instructions are never stepped. Exec
already supplies its source-defined trap at the new entry. A debugger's step
over fork applies to the traced parent; the new untraced child has TF cleared,
matching tracing non-inheritance and avoiding an accidental fatal child trap.

Signal delivery follows V7 `sendsig`: a caught signal saves TF in its user
context and clears it on handler entry. Sigreturn can restore the pending
step, which traps after the next original user instruction. It does not trap
merely because sigreturn restores TF. Faults encountered by a stepped
instruction stop with their actual signal; as in V7, TF need not be cleared
until a trace/breakpoint trap. A debugger can clear it through request 6 when
redirecting execution after such a fault.

## Acceptance

Run `make test-debugger`. Host tests check executable-alias copies, permission
preservation, kernel/unmapped/RO-NX rejection and full-range atomic rejection.
Disposable-root QEMU tests at 16/128 MiB check:

- Repeated steps and exact PC/register results, syscall and fork stepping,
  invalid-step rollback, register-enabled TF, faults, handler/sigreturn and
  the first instruction following an exec trap.
- An INT3 insertion, trap, restoration, rewind, step and continue cycle in a
  real filesystem executable; caught/ignored untraced breakpoints.
- Data writes and private VM isolation, shared/sticky text rejection, ordinary
  ETXTBSY guards, unchanged executable checksums, fresh-exec originals and
  fork inheritance of private patches.
- Actual RX write faults and NX execution faults after debugger writes.

The private test gate installs a disposable trace image only while root-exec
tests are active. No debugger probe is installed in the normal root image.
The required marker is `NEXIX_DEBUGGER_OK`, followed by task 57 orphan and
root process/file/TTY/page/filesystem cleanup and boot markers. Working IDE
storage is preserved. Public layout/export and inventory checks also run;
`make test-unit`, shell and cold-boot storage tests provide regression coverage.

# Bootstrap validation environment

Initial observed boot: Debian 13, x86_64 host GCC 14.2.0, GNU binutils 2.44,
QEMU 10.0.13, GRUB 2.12, and xorriso 1.5.6. These versions record the tested
environment; they are not yet a pinned cross-toolchain or reproducible SDK.

Run `make TOOLCHAIN_PREFIX= test` in this environment. The suite checks:

- ELF64 load-segment bounds, permissions, and the Multiboot2 header.
- Successful long-mode entry and return from ten separate timer interrupts.
- Fatal invalid-opcode exception (vector 6, zero normalized error code).
- Fatal page fault (vector 14, zero read/nonpresent/supervisor error code,
  CR2 equal to the unmapped address `0x40000000`).
- VM remains alive until the test harness terminates it; unexpected exits fail.
- Host tests for bounded boot tag parsing, malformed/duplicate/extended maps,
  address overflow, overlapping reservations, partial pages, invalid frees,
  allocation uniqueness, exhaustion, and reuse.
- QEMU allocation/free self-test during normal boot. Additional 16 MiB and
  128 MiB runs exhaust physical pages, write every byte, reclaim all pages,
  restore the original free count, and preserve boot data and a loaded module.
- Paging self-test: effective kernel permissions, null guard, independent maps
  at the same virtual address, CR3 switching, TLB invalidation, executable code
  mapping, invalid inputs, mappings across page-table levels, and full private
  table reclamation without freeing caller-owned data frames.
- Hardware protection faults: write to kernel text (error 3), NX instruction
  fetch (error 17), and reads from unmapped/null pages (error 0). Each must report
  the expected CR2 address. These tests execute at CPL0, with CR0.WP enabled.
- Permission-change invalidation: cache an executable mapping, change it to NX,
  and require an instruction-fetch fault; cache a writable mapping, change it to
  read-only, and require a write-protection fault.
- Page-table exhaustion at 16 MiB: failed address-space creation and partial
  mapping allocation restore the free count and leave no mapping behind.
- Actual CPL3 execution in two address spaces with different TSS.RSP0 stacks:
  user stack writes, general-register and direction-flag preservation across
  trap return, at least ten timer IRQs per context, kernel continuation return,
  and complete test-context memory reclamation.
- User protection faults: reading supervisor kernel memory (PF error 5),
  writing RX user code (PF error 7), and executing the NX user stack (PF error 21).
  These require CS=`0x23`, SS=`0x1b`, a validated kernel entry stack, and expected CR2.
- User `cli` and port I/O cause GP error 0. User `int 0x20` cannot invoke a
  kernel-only gate and causes GP error `0x102`.
- An unmapped user RSP still allows entry through the TSS kernel stack.
  An intentionally invalid TSS.RSP0 produces a double fault with diagnostics
  on the independently guarded IST1 stack, without a triple-fault VM exit.

The serial transcripts are retained in `build/boot.log`, `build/ud.log`, and
`build/pf.log`, plus `build/memory-16M.log` and `build/memory-128M.log`.
Test images are separate from the ordinary boot image.
Additional paging logs are `build/wp.log`, `build/nx.log`, `build/unmap.log`,
`build/null.log`, `build/protect.log`, and `build/paging-oom.log`. `make TOOLCHAIN_PREFIX= test-paging`
runs the protection and allocation-failure cases separately.
`make TOOLCHAIN_PREFIX= test-user` runs the user protection and stack cases;
their transcripts are `build/user-*.log`. Normal boot must also emit all four
`NEXIX_USER_*_OK` markers. Independent groups can run concurrently with
`make -j3 TOOLCHAIN_PREFIX= test`.
The host parser/allocator tests also pass AddressSanitizer and UBSan with leak
detection disabled (LeakSanitizer cannot run under this session's ptrace).
The suite validates native ELF loading and execution, but does not yet validate
filesystems or rebuilt V7 userland execution.

V7 type adaptation (step 7): `make TOOLCHAIN_PREFIX= test-unit` also runs
`build/v7-types-test`. It checks independent PDP-11 byte fixtures, rejection of
24-bit address overflow without mutation, inode-block boundaries through inode
65535, device encoding, and preservation of native addresses and offsets.
Layout assertions compile in both host tests and the kernel. See
[v7-types.md](v7-types.md) for runtime/disk widths and deferred machine features.

Process scheduling and lifecycle (step 8): every successful boot checks all eight
`NEXIX_PROCESS_*_OK` markers. `make TOOLCHAIN_PREFIX= test-process` repeats the
normal image with 16 MiB RAM and retains `build/process-16M.log`. Tests cover:

- All callee-saved integer registers across an explicit context switch,
  including preservation of an IRQ-disabled caller.
- Two kernel workers that require PIT preemption to complete a handshake.
- Three wait-channel sleepers: wake all matching sleepers, preserve unrelated
  sleepers, and return zero on repeated wakeups.
- Idle scheduler wakeup after a deadline, blocking wait, ECHILD, encoded exit
  status, cumulative accounting, and an orphan adopted by PID 1.
- All free process slots reserved and aborted, with EAGAIN at capacity and no
  leaked VM roots, plus the non-root 25-process limit.
- Ring 3 fork with independent code, stack, data, read-only, and distant mappings,
  preserved permissions and bytes, inherited credentials and signal addresses,
  rebased per-process pointers, and correct parent/child return values.
- Rejection of unsupported file references before copying any pages.
- Allocation failure at each of the sixteen allocation boundaries in that
  fork image; every failure restores the free count and leaves the parent intact.
- Parent/child user-mode timer preemption and register preservation, independent
  stack/data writes, wait status, and child accounting.
- Final recovery of every process-owned data/table page and every process slot.

The process test gate uses `int 0x81` commands only; the native ABI uses `int 0x80`. Tests use integer contexts without FPU/TLS. See [processes.md](processes.md).

Native syscalls and safe user access (step 9): successful boot requires
NEXIX_SYSCALL_ABI_OK, NEXIX_UACCESS_OK, NEXIX_SYSCALL_ERRORS_OK,
NEXIX_SYSCALL_FORK_OK and NEXIX_SYSCALL_OK. `make TOOLCHAIN_PREFIX= test-syscall`
boots the ordinary image with 16 MiB and saves `build/syscall-16M.log`; the full
suite includes this run. `make test-process` remains available for the same
16 MiB normal boot with `build/process-16M.log` as its transcript.

The new host user-copy tests use independent mappings and noncontiguous backing,
cover full-range atomic rejection, effective supervisor/read-only permissions,
overflow and canonical-range limits, zero length, NUL termination at boundaries,
capacity exhaustion, and partial string failure. They pass AddressSanitizer and
UBSan with leak detection disabled. Ring 3 tests exercise six full-width argument
registers, every preserved GPR, CF/DF/IF, error clearing, supported identity/mask
calls, high-bit and unsupported syscall numbers, invalid output addresses,
unaligned cross-page output, and a native fork/wait/exit sequence. The child
confirms the parent is blocked in wait before exiting; an earlier invalid status
pointer leaves the child available to reap. Parent and child memory remain
independent, tick accounting is collected, and all process resources are freed.
See [syscall-abi.md](syscall-abi.md) for the ABI and exact service scope.

ELF64 execution (step 10): every successful boot requires `NEXIX_ELF_OK`,
`NEXIX_EXEC_ROLLBACK_OK` and `NEXIX_EXEC_OK`. `make TOOLCHAIN_PREFIX= test-exec`
runs the complete normal boot with 16 MiB RAM (`build/exec-16M.log`), including
malformed ELF rejection, partial-allocation rollback, argument/environment
stacks, and an actual CPL3 fork/exec/wait sequence. See [exec.md](exec.md).
The full suite uses test-exec for its low-memory normal boot; test-process and
test-syscall remain available separately.

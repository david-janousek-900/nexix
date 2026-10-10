# Architecture and porting boundaries

The bootloader enters a 32-bit assembly stub at 1 MiB. The stub checks long-mode
support, installs an initial GDT and page tables, enters 64-bit mode, and calls C
using the System V AMD64 calling convention and a 16-byte-aligned stack.
All initial load segments must reside below 1 GiB. Mapping the entire first GiB
does not imply that all mapped addresses are available RAM.

The assembly bootstrap mapping is replaced by managed 4 KiB page tables after
physical allocation is initialized. EFER.NXE enables execute protection and
CR0.WP enforces read-only mappings even for kernel-mode writes. A CPU without
NX support is rejected. Kernel text is read-only/executable; the Multiboot header
and rodata are read-only/NX; remaining identity-mapped memory is writable/NX.
Page zero is unmapped. These permissions follow page-aligned linker boundaries.

The current virtual layout is:

| Region | Purpose |
| --- | --- |
| `0 .. 1 GiB` | Shared supervisor-only identity map, with the null-page guard |
| `1 GiB .. 128 TiB` | Independent user mappings in the lower canonical range |
| Upper canonical range | Unmapped; not yet supported by the mapping API |

Every address space owns its PML4 and initial PDPT. The PDPT entry for the first
GiB references a shared kernel directory and remains supervisor-only, including
when parent entries permit user access. Other branches are allocated on demand.
This keeps kernel code, stacks, IDT, allocator state, and page-table access valid
across CR3 switches. The initial identity map uses 515 allocated page-table pages.

`vm_create`, `vm_map`, `vm_protect`, `vm_unmap`, `vm_translate`, `vm_activate`,
and `vm_destroy` provide the address-space interface. Mapping/protection changes
accept aligned user addresses only, reject write+execute permissions, and never
overwrite an existing mapping. Physical frames must be allocated by the page
allocator. This is a trusted kernel API, not a user-callable interface.

Active mapping changes invalidate the address's TLB entry; switching spaces
reloads CR3. Empty user tables are reclaimed on unmap. Creation and mapping
failures roll back newly allocated tables. Destruction rejects active/kernel
spaces, frees private tables, and preserves the shared kernel directory.
Mapped data frames remain caller-owned and must be unmapped before freeing.
All current paging operations require interrupts disabled; SMP and concurrent
page-table mutation are deferred.

User-mode entry and actual CPL3 isolation tests now exercise these mappings.
The original paging self-test still runs at CPL0.

The Multiboot2 header requires a memory map. The parser bounds all tag reads,
validates map record stride/version and address arithmetic, and requires the end
tag. Extended map records are accepted; malformed or duplicate maps are rejected.

The physical allocator uses separate eligibility and allocation bitmaps for
4 KiB pages below 1 GiB. Only complete pages in available RAM are eligible;
overlapping firmware-reserved entries take precedence. Any page overlapping
`[0, kernel_end)`, the boot information buffer, or a loaded module is reserved.
`kernel_end` covers kernel code, data, stack, page tables, and allocator bitmaps.
Memory above the bootstrap mapping limit remains unmanaged.

`page_alloc` returns a physical address or zero on exhaustion; `page_free`
rejects reserved, unaligned, out-of-range, and already-free pages. Returned pages
are not zeroed. Initialization and the current callers run with interrupts
disabled; future concurrent callers must serialize access. The boot buffer and
modules remain reserved until an explicit reclamation mechanism is implemented.

The IDT contains 256 interrupt gates. Vectors `0x80` (native syscalls) and
`0x81` (bootstrap tests) are callable by user
code for the bootstrap test harness. Assembly stubs normalize
exception error codes, save all general registers, align the C call stack, and
restore interrupted execution with `iretq`. Exceptions report vector, error,
RIP, and (for page faults) CR2 before halting with interrupts disabled.
The PIC uses vectors 32..47, with only IRQ0 enabled. PIT channel 0 drives a
roughly 100 Hz tick counter. Interrupt handlers acknowledge the PIC; serial
logging stays outside the timer handler. Spurious IRQ7/IRQ15 are handled.

The active GDT has kernel code/data selectors `0x08`/`0x10`, user code/data
selectors `0x23`/`0x1b` (including RPL), and a 64-bit TSS at selector `0x28`.
`kernel_stack_set` updates TSS.RSP0 for the current context. Two static test
contexts have separate 16 KiB entry stacks. Double faults use IST1; NMIs use
IST2. Each stack has an unmapped guard page below it. The I/O bitmap offset
lies beyond the TSS limit, denying user port I/O with IOPL zero.

`arch_user_test_enter` builds an `iretq` frame with IF=1, IOPL=0, and cleared
general registers except the test argument in RDI. User code runs in its own
address space with RX code, a zeroed RW/NX stack, and an unmapped page below
the user stack. Interrupt entry loads kernel data selectors and clears DF;
`iretq` restores user selectors, flags, and registers on ordinary trap/IRQ return.
The C handler validates that CPL3 frames landed on the configured kernel stack.
Fatal diagnostics include CS, SS, saved RSP, and the kernel frame address.

The `0x81` test gate provides checkpoint, tick-query, and completion operations
only while a bootstrap test context is active. Completion explicitly restores
the saved kernel C continuation with interrupts disabled. This single-core,
non-reentrant harness is not a scheduler or a published syscall ABI. Successful
contexts destroy their address spaces and release their frames; failures panic.
The tests establish register/DF preservation, hardware privilege checks, and
double-fault entry on IST1 despite an invalid RSP0. The NMI IST is configured
but has no dedicated hardware NMI injection test yet.

V7 process/type definitions and a native process scheduler are implemented.
The scheduler changes CR3, current-process ownership, and TSS.RSP0 with interrupts
disabled before switching integer contexts. Kernel and user tasks are preempted
at five PIT ticks; sleeping tasks use V7-style wait channels. Eager fork copies
private user pages and resumes a cloned user trap frame. Exit leaves a zombie;
the scheduler releases its VM after switching stacks, and wait recycles its slot.
See [processes.md](processes.md) for the source adaptation, ownership rules,
and tests. The [native syscall ABI](syscall-abi.md) uses the DPL3 `0x80` interrupt
gate and saved-frame IRETQ return. User-copy helpers validate effective user
permissions and full ranges before copyin/out through the physical identity map.
Process fault handling, ELF loading, FPU state, and user TLS remain pending.

Keep the historical source tree pristine. Ported code belongs in `kernel/` and,
later, `libc/` and `user/`. Reuse V7 process, file, and filesystem logic after
auditing PDP-11 machine dependencies; replace assembly and hardware interfaces.

Audit historical integer widths, implicit declarations, pointer/integer casts,
alignment, varargs, signal frames, and compiler assumptions. Encode disk records
explicitly instead of writing native structures: LP64 changes cannot silently
change the historical filesystem format.

The syscall register convention, numbers, errors, and exported type widths are
documented in [syscall-abi.md](syscall-abi.md); the signal ABI is still pending.
Native userland uses ELF64;
PDP-11 a.out execution is deferred. Start `fork` with eager copying.

Networking will use an e1000 driver and lwIP's raw callback API. Serialize stack
processing in kernel context with bounded queues, then expose blocking socket
descriptors through V7 sleep/wakeup and file operations. Start with static IPv4
configuration, ARP, ICMP, UDP, and TCP. Pin lwIP and retain its license at import.

Architecture references: [Intel system programming manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
and [Multiboot2 boot information format](https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html).

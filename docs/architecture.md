# Architecture and porting boundaries

The bootloader enters a 32-bit assembly stub at 1 MiB. The stub checks long-mode
support, installs an initial GDT and page tables, enters 64-bit mode, and calls C
using the System V AMD64 calling convention and a 16-byte-aligned stack.
All initial load segments must reside below 1 GiB. Mapping the entire first GiB
does not imply that all mapped addresses are available RAM.

The current mapping is temporary. User/kernel isolation, NX protection, and
4 KiB page-table management are the next architecture tasks.

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

The IDT contains 256 kernel-only interrupt gates. Assembly stubs normalize
exception error codes, save all general registers, align the C call stack, and
restore interrupted execution with `iretq`. Exceptions report vector, error,
RIP, and (for page faults) CR2 before halting with interrupts disabled.
The PIC uses vectors 32..47, with only IRQ0 enabled. PIT channel 0 drives a
roughly 100 Hz tick counter. Interrupt handlers acknowledge the PIC; serial
logging stays outside the timer handler. Spurious IRQ7/IRQ15 are handled.

This is a kernel-only interrupt foundation: no user-mode stack switching, TSS,
or dedicated IST stack exists yet. Stack-corruption/double-fault recovery must
be added before relying on diagnostics for those cases. The bootstrap is not
yet a completed machine-support layer.

Keep the historical source tree pristine. Ported code belongs in `kernel/` and,
later, `libc/` and `user/`. Reuse V7 process, file, and filesystem logic after
auditing PDP-11 machine dependencies; replace assembly and hardware interfaces.

Audit historical integer widths, implicit declarations, pointer/integer casts,
alignment, varargs, signal frames, and compiler assumptions. Encode disk records
explicitly instead of writing native structures: LP64 changes cannot silently
change the historical filesystem format.

Before implementing user execution, document the syscall register convention,
numbers, errors, exported type widths, and signal ABI. Native userland uses ELF64;
PDP-11 a.out execution is deferred. Start `fork` with eager copying.

Networking will use an e1000 driver and lwIP's raw callback API. Serialize stack
processing in kernel context with bounded queues, then expose blocking socket
descriptors through V7 sleep/wakeup and file operations. Start with static IPv4
configuration, ARP, ICMP, UDP, and TCP. Pin lwIP and retain its license at import.

Architecture references: [Intel system programming manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
and [Multiboot2 boot information format](https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html).

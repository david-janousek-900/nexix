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
The PIC uses vectors 32..47, with IRQ0 (PIT) and IRQ4 (COM1 receive) enabled. PIT channel 0 drives a
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
ELF64 loading builds private staged images and a native argc/argv/envp stack.
Exec activates a complete new image before releasing the old VM; failures leave
the old process runnable. Production exec loads ELF images from the mounted filesystem. An embedded
program catalog remains for isolated bootstrap fixtures. See [exec.md](exec.md). Native crt0, a static
libc archive and a C fixture exercise the same startup stack and syscall ABI;
see [libc.md](libc.md). Managed user faults deliver signals. Native FP state
is preserved across process switches, fork, exec and signals (tasks 25–27);
see [floating-point.md](floating-point.md). User TLS remains outside the current ABI.

Keep the historical source tree pristine. Ported code belongs in `kernel/`,
`libc/`, `libm/` and `user/`. Reuse V7 process, file, and filesystem logic after
auditing PDP-11 machine dependencies; replace assembly and hardware interfaces.

Audit historical integer widths, implicit declarations, pointer/integer casts,
alignment, varargs, signal frames, and compiler assumptions. Encode disk records
explicitly instead of writing native structures: LP64 changes cannot silently
change the historical filesystem format.

The syscall register convention, numbers, errors, and exported type widths are
documented in [syscall-abi.md](syscall-abi.md); the signal ABI is documented in [signals.md](signals.md).
Native userland uses ELF64;
PDP-11 a.out execution is deferred. `fork` uses eager copying.

Networking is planned as a direct 4.4BSD-Lite2 stack port with an e1000 driver,
replacing the earlier lwIP proposal. Adapt BSD packet buffers, socket state and
protocol timers to Nexix's allocator, file descriptors and V7 sleep/wakeup.
Serialize stack access on the supported single CPU; interrupt handlers queue
bounded work for a schedulable kernel worker. Start with static IPv4, Ethernet
and loopback, ARP, ICMP, UDP, TCP and blocking sockets. Pin upstream inputs,
retain their notices and audit LP64 assumptions. See the
[0.2.0 networking roadmap](networking-roadmap.md) for milestones and acceptance.
The private [compatibility foundation](networking-compat.md) supplies fixed-width
protocol types, byte-array wire records, byte-order/checksum helpers, sequence
comparisons and checked size arithmetic. Bounded
[packet and socket pools](networking-pools.md) replace BSD VM-backed mbufs, and a serialized
[network worker](networking-worker.md) runs protocol work and timers. [PCI discovery](networking-pci.md) finds and maps
the QEMU e1000 through an uncached MMIO window, and its
[DMA driver](networking-e1000.md) hands frames to the worker, where
[interfaces](networking-interfaces.md) frame and demultiplex them and
[static routes and ARP](networking-routing.md) choose next hops for
[IPv4 datagrams](networking-ip.md) and
[ICMP and raw echo sockets](networking-icmp.md); hardware and stack integration remain
planned work.

Architecture references: [Intel system programming manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
and [Multiboot2 boot information format](https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html).

## Block and filesystem foundation

The synchronous 512-byte block interface includes a RAM-backed device. V7
filesystem mount, inode decoding, lookup, direct and triple-level indirect I/O,
sparse growth and chained free-block allocation use the retained disk formats.
A 29-sector cache uses synchronous writes. Callers serialize whole operations;
write errors disable the mount until remount. Boot tests temporarily mount a
generated RAM fixture and execute an ELF read through these APIs. Normal boot
mounts the production root on the primary IDE master, with a RAM fallback when
absent, streams ELF segments from inodes and launches init as PID 1; see [booting.md](booting.md).
The file layer now manages descriptors and inode ownership across fork/exec/exit,
with checked namespace operations and blocking pipes; see [files.md](files.md). See
[filesystem.md](filesystem.md) for API contracts and format limits.


## Serial terminal

COM1 receive and line-status interrupts feed a bounded V7-style terminal input
queue. Cooked editing/records, raw/cbreak input, echo, CR/tab processing and
software flow control use the process sleep/wakeup interface. Transmit remains
polled; no heap allocation or sleeping occurs in the receive IRQ handler.
Character-device descriptors use the file layer's ownership across fork/exec/exit.
Native ioctl and legacy stty/gtty export eight-byte sgtty and six-byte tchars
records. Controlling-terminal association, the demo, test coverage and remaining
signal/job-control work are documented in [tty.md](tty.md).

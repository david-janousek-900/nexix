# 4.4BSD and Nexix kernel analysis

Nexix should keep its native x86_64 kernel and V7 compatibility, while adopting
selected 4.4BSD abstractions for memory ownership, faults, files and I/O.
The strongest next steps are reducing fixed memory overhead, adding VM region
and frame ownership metadata, and implementing copy-on-write fork and demand-zero
anonymous memory. File-backed paging and swap belong after the locking and I/O
changes that make them safe.

Analyzed Nexix baseline: `37c84b8`. The BSD comparison uses 4.4BSD-Lite2 mirror
commit `50587b00e922225c62f1706266587f435898126d`, the same full archive pinned by
[the BSD reference audit](bsd-networking-audit.md). Its SHA-256 is
`5a851582c4c3a5053ef2f4f41df347996969a95c3c7236b0b9e99319ff02c3ef`.
This proposal concerns architecture and implementation order; its memory
calculations are layout estimates, not measured performance improvements.

Implementation ownership is in [tasks 456–484](tasks.md#kernel-evolution-after-the-44bsd-review)
and the broader [BSD kernel enhancement tasks 504–551](tasks.md#enhance-the-native-kernel-with-44bsd-lite2-features).
Signal/job-control and file-locking work is shared with the nvi replacement
rather than implemented twice.

## What the current kernel already provides

The current kernel is a native V7-derived system, rather than the original
PDP-11 kernel. It already has four-level page tables, independent user address
spaces, hardware protection, NX, write-xor-execute user mappings, guarded stacks,
ELF64 exec, preemptive processes, signals, tracing, mounted filesystems and BSD
networking. Adopting virtual memory therefore means extending the existing
paging model with backing objects and recoverable faults.

| Area | Original V7 reference | Current Nexix | Useful 4.4BSD direction |
|---|---|---|---|
| Address spaces | PDP-11 segmentation and machine-dependent process images | Native x86_64 page tables and private resident pages | Separate logical VM maps from architecture-specific translations |
| Memory pressure | Process/text swapping | Allocation failure; ordinary process memory remains resident | Reclaimable pages, pagers and explicit pressure policy |
| Fork | Copies process memory, with swapping paths | Eagerly copies ordinary user leaves; physical mappings share separately | Share pages until a private write requires a copy |
| Executables | Text ownership and shared-text machinery | ETXTBSY ownership records, but private ELF frames | Immutable executable backing objects and shared clean code pages |
| Heap and stack | Segmentation-dependent expansion | Eager brk allocation and eight initially allocated stack pages | Demand-zero regions and bounded stack growth |
| Filesystems | Inode-centric V7 disk format | V7 filesystems with multiple mounts and device/inode identities | A vnode-style interface independent of disk format |
| File I/O | Buffer cache and historical drivers | Small write-through cache and synchronous polling IDE I/O | Explicit request/completion ownership and controlled caching |
| Scheduling | V7 priorities, aging and wait channels | Priority selection, decay and a five-tick quantum on one CPU | Better run/wait queues after long critical sections are addressed |
| Readiness | No BSD descriptor multiplexing API | Nonblocking sockets exist; select/poll is absent | File-operation readiness hooks and atomic wait registration |

Evidence: [paging API](../kernel/include/paging.h), [process lifecycle](../kernel/v7/process.c),
[ELF loader](../kernel/elf.c), [text ownership](text-ownership.md),
[mounts](mounts.md), [socket operations](../kernel/v7/socket.c), and
[original V7 swapping](../reference/v7/usr/sys/sys/text.c).
Current scope includes nested mounts, chroot and tracing stops. Upper canonical
addresses already contain the dedicated supervisor MMIO window; the general
kernel-VA allocator proposed below would be additional infrastructure.

## What to take from 4.4BSD

4.4BSD separates address ranges, backing storage, resident pages and hardware
mappings. Its Mach-derived VM supports sharing and mapped files; this is an
architectural model worth adopting without importing an entire historical
kernel. The [published 4.4BSD design overview](https://docs.freebsd.org/en/books/design-44bsd/)
explains that separation and the distinction between private and shared mappings.

The pinned sources make the divisions concrete:

| BSD abstraction | Responsibility | Native counterpart to introduce |
|---|---|---|
| [vm_map and entries](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vm_map.h) | Valid ranges, protections, inheritance and object offsets | A per-process region map above the existing page-table API |
| [vm_object](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vm_object.h) | Backing identity, resident pages, references and shadows | Anonymous and executable objects with explicit lifetime |
| [vm_fault](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vm_fault.c) | Resolve a legal missing page or private write | One fault resolver used by traps and user-copy helpers |
| [pmap](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/i386/i386/pmap.c) | Hardware translations and mapping attributes | Keep and evolve Nexix's x86_64 implementation |
| [pager and pageout](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vm_pageout.c) | Obtain/write backing data and maintain free-page supply | Bounded pager operations, reclaim queues and emergency reserves |
| [vnode layer](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/sys/vnode.h) | File identity and filesystem-independent operations | A small native interface wrapping the existing V7 backend |

Preserve Nexix's explicit disk/wire encoders, W^X rules and tested ABI. Historical
BSD pointer widths, pmap code and spl/locking assumptions require adaptation.
4.4BSD-Lite2 is not a source for modern FreeBSD UMA, kqueue or a modern SMP
kernel. Its vnode pager and filesystem buffer cache are separate mechanisms;
a coherent unified file cache would be additional Nexix work. The selected FFS
sources should not be described as a journaled or soft-updates filesystem.

The repository now retains the complete pinned BSD mirror under
`reference/bsd-lite2/source/`, including the VM/VFS files examined above.
Full-tree provenance is verified; dependency, locking and per-component license
audits still need expansion before adding active adaptations (task 456).

## First recover memory that is already reserved

### Make the kernel direct map proportional to managed memory

[paging_init](../kernel/arch/x86_64/paging.c) allocates 512 leaf page-table pages
to map the entire first GiB with 4 KiB leaves. Together with three root/intermediate
pages and one initial MMIO PDPT, startup allocates 516 table pages, approximately
2.016 MiB, before later mappings. This happens even in a 16 MiB guest.

Build the managed direct map from the RAM ranges and required kernel, boot and
device ranges. Keep every allocator frame accessible through its kernel alias,
including frames used for new page tables. Preserve the null guard, kernel
RX/RO/NX boundaries, stack guards and uncached VGA mapping.

For a contiguous 16 MiB mapping, eight leaf tables instead of 512 would save
504 pages, about 1.97 MiB. For 128 MiB, the corresponding estimate is 1.75 MiB.
Actual savings depend on reserved ranges and layout. This is a smaller first
project than demand paging and can improve the existing low-memory profile.

Large supervisor pages are another option later. They require extending walkers,
translation and splitting logic: the current walkers reject large leaves. Do
not trade away fine-grained protection just to reduce table overhead.

### Allocate process stacks when processes exist

[process.c](../kernel/v7/process.c) statically reserves 150 stack slots, each
containing a 16 KiB stack plus a 4 KiB guard reservation:

    150 × 20 KiB = 3000 KiB ≈ 2.93 MiB

That is about 18.3% of a nominal 16 MiB machine before other kernel costs.
Unused slots retain their physical reservation even when their guard page is
unmapped. The per-slot process and user tables are additional storage.

Introduce a shared supervisor kernel-VA allocator and map four physical pages
per live stack, leaving its guard unbacked. Keep stacks wired, zero them on reuse,
and release them only after switching to the scheduler stack. Their mappings
must exist under every process CR3 and remain distinct from the MMIO arena.
Removing the stack array from reserved BSS is necessary; merely unmapping its
pages does not make them eligible for the current allocator.

Keep the process-count limit initially. Saving idle stack backing does not
require simultaneously changing PID, file-table or process-publication rules.

## Build virtual memory in stages

### 1. Establish frame ownership and logical regions

The current [allocator](../kernel/memory.c) records eligibility and allocation
bits. Ordinary process leaves are exclusively owned; teardown calls page_free.
That model cannot safely express shared code, COW or resident cache pages.

Add explicit ownership references, pin counts and frame classes. Distinguish
ordinary anonymous memory, file-backed pages, page tables, kernel stacks and
DMA/device memory. Last-reference release must not free a pinned frame. Audit
every teardown path: failed fork, failed exec, brk shrink, exit, debugger writes
and physical mappings. VM_PHYS already represents a separate shared/device
lifetime and must retain its semantics.

Add a region map containing start/end, logical and maximum protection, backing
object/offset, inheritance and stack/guard policy. Initially keep all existing
pages eager and resident. A bounded sorted list is adequate for the first
implementation; a balanced tree can follow measured lookup pressure. Limit
region count and define split/merge behavior so mmap churn cannot exhaust the
kernel unpredictably.

Avoid allocating metadata for every possible PFN below 1 GiB on a 16 MiB machine.
For illustration, 16 bytes for each of 4096 RAM pages is 64 KiB; the same record
for all 262144 possible frames costs 4 MiB. Allocate metadata for actual managed
RAM or use sparse sections. These figures exclude other VM structures.

### 2. Make faults and user-copy helpers understand VM policy

Today, a managed user page fault becomes a signal; there is no recovery path
that materializes an anonymous page or resolves COW. Add a resolver that first
checks region permissions and access type, then handles valid missing pages or
COW, and signals genuine invalid/protection accesses. Reserved-bit faults and
unrelated kernel faults must retain their diagnostic handling.

The most important integration point is [uaccess.c](../kernel/uaccess.c).
copyin/copyout walk page tables and copy through physical aliases. They do not
naturally cause a user virtual-address fault. A PTE-only COW patch would either
make copyout reject valid buffers or let kernel writes alter shared parent/child
memory.

Introduce a fault-and-pin preparation step for kernel reads and writes to user
ranges. It must honor logical permissions, instantiate valid lazy pages and
privatize writable COW destinations before copying. Preserve existing
whole-range validation and no-partial-output contracts where promised. Internal
page preparation may change residency while still leaving user bytes untouched
on failure; document that distinction.

Apply this to read/recv results, signal frames, wait/status outputs, ioctl/kinfo
records and tracing. Debugger instruction writes require a private executable
page even when its ordinary protection is RX. Core dumps must use logical maps
and a defined policy for nonresident pages, without reading device mappings.

### 3. Add COW fork and shared immutable code

[clone_mapping](../kernel/v7/process.c) currently copies every ordinary user
leaf, including RX code. Adapt the ideas in BSD's
[vmspace_fork](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vm_map.c#L2112):

1. Share immutable pages by reference.
2. Share private writable pages read-only, recording that they are logically
   writable and require COW.
3. Write-protect both parent and child mappings and invalidate affected parent
   TLB entries before it resumes.
4. On a private write, copy the page or regain write permission if ownership
   proves it is no longer shared.
5. Roll back failed child construction without losing references or leaving
   parent permissions semantically wrong.

Preserve VM_PHYS sharing and the existing executable-inode ETXTBSY contract.
The current text table shares ownership records, not physical code frames.
The first COW implementation will still walk mappings and allocate child page
tables; its benefit is avoiding bulk data copying, not constant-time fork.

Illustration: a 2 MiB private image copied eagerly occupies roughly 4 MiB across
parent and child. With COW, a child modifying two pages initially adds about
8 KiB of data backing. Page tables, stacks and VM metadata are extra.

Choose a commit/OOM policy explicitly. Strict reservation preserves stronger
up-front guarantees but limits admission gains. Controlled overcommit benefits
fork-then-exec workloads, but requires a documented fault-time OOM policy. An
invalid mapping is different from a legal COW fault that cannot allocate; do not
silently conflate their signals or accounting.

### 4. Add demand-zero heap and bounded stack growth

[proc_brk](../kernel/v7/process.c) eagerly allocates and clears every new heap
page, while [elf.c](../kernel/elf.c) eagerly materializes segment memory and the
initial user stack. Reserve anonymous virtual ranges and allocate a zeroed frame
on first access. A shared read-only zero page can optimize reads later.

Keep the initial stack ABI unchanged while first testing lazy heap/BSS. Add
stack growth separately, with a maximum region and a moving guard; arbitrary
writes below the stack must not authorize unlimited growth. Zero all newly
exposed bytes, including partial-page brk tails and reused frames.

Distinguish virtual size, committed backing and resident/shared pages in a new
versioned kinfo record. The current p_size accounting is based on mapped bytes;
changing it silently would make ps and existing tests misleading.

### 5. Introduce file-backed paging and mmap after I/O ownership

Use immutable executable files as the first file-backed objects. Pin the backing
inode for the mapping lifetime and preserve exclusion against mutation. Handle
unaligned ELF segment edges and zero the final partial page correctly.
Transactional exec must still preserve the old image and credentials on staging
failure; subsequent backing-I/O failures need an explicit fault-time policy.

An anonymous-only mmap/munmap/mprotect interface can follow region and
anonymous-fault support independently of filesystem I/O. Add file private
mappings after backing-I/O ownership and invalidation work, and shared writable
mappings last. Append/version native interfaces without renumbering existing
calls. A new syscall alone does not provide pager and coherence semantics.

File mappings need defined behavior for close, fork, unlink, truncate, unmount
and simultaneous ordinary file I/O. A mapped object must keep its backing alive
after its original descriptor closes. Prevent stale cached pages when an inode
is truncated or reused. Preserve W^X and reject mapping overlaps with physical
windows and guards.

The pinned [vnode pager](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/vm/vnode_pager.c)
uses vnode references, size checks and potentially blocking operations. Its
implementation explicitly limits its basic I/O routine to one page. Adopt the
ownership model; do not assume modern clustered or unified-cache behavior.

### 6. Reclaim clean pages before adding swap

Add access/dirty tracking, bounded reclaim work, low/high free-page watermarks
and reserves for page tables, COW, fault handling and I/O completion. Clean,
reconstructible file-backed pages are the first useful reclaim class. Dirty
anonymous pages cannot simply be discarded without backing storage.

Swap should follow working page ownership, a backing-object model, safe I/O and
an OOM policy. Use a dedicated swap extent/device initially, avoiding recursive
filesystem allocation in the pageout path. Test full swap, I/O errors, dirty-page
races and repeated page-in/out. Keep kernel stacks, page tables, active DMA
buffers and explicitly wired pages outside ordinary eviction.

A clock/second-chance policy and a bounded worker are sufficient first steps.
Copying BSD's entire object-shadow/pageout implementation would introduce more
lifetime and locking complexity than Nexix needs initially.

## Improvements beyond VM

### Shorter critical sections and explicit locks

Large fork, ELF, user-copy and disk operations rely on interrupt serialization.
This can delay unrelated timer, terminal and network work even on one CPU.
Measure maximum IRQ-off duration and I/O wait latency before changing scheduling
policy.

Nexix can already switch away from a sleeping task with IRQs disabled; that does
not make arbitrary filesystem or VM state safe to expose across a sleep. Add
explicit map, object, inode and I/O ownership rules, lock order, wait-channel
registration and assertions for operations that may sleep. Drop relevant locks
before backing I/O and revalidate references/map versions afterward. Reuse the
network worker pattern for bounded deferred work, while avoiding one global
worker becoming a serial bottleneck.

### A filesystem-independent vnode layer

Wrap the current device/inode implementation with a small operation interface
for lookup, attributes, read/write, inactive/reclaim and pager access. Keep the
V7 filesystem as the first backend. Existing mounted namespaces already solve
part of the lifetime problem, so this can be incremental.

This opens a path to tmpfs, an alternative disk filesystem and longer names
without rewriting process/file-descriptor semantics. Changing DIRSIZ or native
inode widths globally would corrupt compatibility with existing V7 images.
Add any new format with separate tooling and a migration plan.

### Storage completion, cache policy and recovery

The [IDE driver](../kernel/ide.c) polls and disables device interrupts. The V7
filesystem has 29 cached 512-byte sectors per instance, about 14.5 KiB, with
write-through behavior. Its historical HP queue normally has only one request
under the current synchronous path; it is not evidence of concurrent I/O.

First add request/completion ownership, sleeping waits and fault injection.
Then improve read-ahead and cache replacement using measured hit rates. Delay
writeback only after defining dirty-buffer lifetime, error reporting, flush
ordering and recovery. A completed ATA flush does not make a multi-sector
filesystem mutation transactional.

A read-only consistency checker and disposable-image crash tests are valuable
before a more aggressive write policy. BSD FFS offers allocation/locality and
metadata-ordering examples, not a journal that can simply be transplanted.

### Descriptor readiness and resource controls

Add file-operation readiness hooks and poll/select with registration plus
condition recheck under the same synchronization. Existing nonblocking socket
support is a starting point; pipes and TTYs need consistent behavior too. This
lets network tools avoid forked relay processes or timeout-only waiting.
BSD's [select implementation](https://github.com/sergev/4.4BSD-Lite2/blob/50587b00e922225c62f1706266587f435898126d/usr/src/sys/kern/sys_generic.c)
provides a concrete reference for event registration and wakeups.

Introduce configurable per-process limits for address space/data, stack, files,
processes and pinned memory, with checked system-wide budgets. Do not merely
increase the current 150 process slots, 20 descriptors per process, 175 file
entries and 200 cached inode entries. Preserve old ABI record layouts while
adding versioned limit and VM statistics interfaces.

### Scheduling and later hardware work

At 150 slots, linear scheduler scans may be adequate. Prioritize eliminating
long interrupt-masked work over replacing the scheduler. If measurements justify
it, add priority run queues, indexed wait queues, better interactive accounting
and explicit starvation tests while preserving nice/accounting contracts.

Managing RAM above 1 GiB requires replacing assumptions that a physical address
is directly dereferenceable, extending mapping/allocator bookkeeping and auditing
DMA addresses. Raising PHYSICAL_LIMIT alone is unsafe. APIC/timer work and SMP
come after explicit locking, per-CPU current-process/stack state, allocator
serialization and TLB shootdown. They are not prerequisites for useful COW or
anonymous demand paging on the supported single CPU.

BSD-style signal masks, foreground process groups and terminal job control are
useful later compatibility work. Existing ptrace stops should not be confused
with complete shell job control. ASLR and additional hardening can follow stable
VM placement and ABI rules; the existing W^X/NX protections should remain.

## Recommended implementation sequence

| Phase | Deliverable | Depends on | Completion evidence |
|---|---|---|---|
| A | Memory/latency counters and memory-map-driven kernel tables | Existing allocator and protection tests | Lower baseline table cost at 16/128 MiB; all kernel/boot/device aliases and permissions preserved |
| B | Kernel VA allocator and dynamic guarded process stacks | Shared kernel mapping ownership | Live-stack accounting, OOM rollback, no active-stack release, repeated process churn returns to baseline |
| C | Frame references, bounded VM regions and fault-aware uaccess | Explicit ownership and accounting | Aliases/failure injection, region split/merge limits, output validation and physical mapping regressions |
| D | COW fork, shared immutable pages and demand-zero anonymous memory; optional anonymous mmap ABI | C | Parent/child isolation, kernel-originated writes, zero-fill and fault-time OOM policy at both memory sizes |
| E | Sleep-safe I/O ownership, vnode adapter and descriptor readiness | Explicit locks; can proceed alongside C/D | Lost-wakeup/close races, storage failures, ordinary file/pipe/TTY/socket readiness |
| F | Executable paging and file mmap support | C/D plus E and cache invalidation | Backing lifetime, partial pages, close/unlink/truncate/unmount rules and coherent reads |
| G | Clean-page reclaim, then optional swap | F, reserves and pressure policy | Sustained pressure, bounded pageout, swap exhaustion/error recovery and no pinned-page eviction |

Do not bundle these into a kernel replacement. Each phase needs its own
ownership contract, focused tests and regression checkpoint. Leave SMP, full
FFS adoption and a large new driver model out of the first VM milestones.

## Tests that should gate the VM work

- Fork a process with a large initialized image; verify shared frame counts,
  then write different pages in parent and child and compare contents.
- Exercise kernel writes into COW/lazy destinations through read, recv, signal
  delivery, wait, ioctl and kinfo, as well as direct user stores.
- Patch shared RX code through ptrace; only the traced process may change, and
  the executable file and other processes must remain unchanged.
- Inject allocation failure at each fork/exec/fault stage; check references,
  parent permissions, unpublished child cleanup and eventual reclamation.
- Grow/shrink lazy memory, reuse frames, approach stack guards and try invalid
  addresses; verify zero-fill and defined fault outcomes.
- Combine physical mappings, DMA/pinned pages, exec, fork, core dumps and memory
  pressure; private VM changes must not alter their existing ownership rules.
- For file paging, race read/write/truncate/unlink/unmount with faults on
  disposable disks. Inject backing errors and verify mount/page-cache policy.
- Run existing process, exec, files, signals, tracing, phys, storage, console,
  stability and TCP/IP acceptance at 16 and 128 MiB. Add larger-memory profiles
  only when allocator/direct-map support is implemented.

Record boot free pages, page-table/stack/metadata bytes, private/shared RSS,
virtual/committed bytes, COW/zero/file faults, copied bytes per fork/exec,
reclaim activity, storage wait latency and maximum IRQ-off duration. These
measurements can determine whether each architectural change actually improves
the supported workloads.

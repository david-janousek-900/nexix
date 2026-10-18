# Native physical mappings (task 64)

V7 syscall 52 is `sysphys` in `usr/sys/sys/machdep.c`, registered in sysent.c.
Its libc wrapper is `usr/src/libc/sys/phys.s`, with `phys(2)` documentation.
`usr/sys/sys/ureg.c` installs ABS mappings from the process segmentation prototype;
`estabur` preserves ABS entries not occupied by the new text/data/stack layout.
Nexix replaces those segment registers with explicit page-table leaves and
reference-counted physical frames. Reference sources remain unchanged.

## Native interface and units

Include `<sys/phys.h>` (also included by `<unistd.h>`):

```
int phys(unsigned slot, size_t pages, uintptr_t physical);
volatile uint64_t *mapping = (void *)NX_PHYS_ADDRESS(slot);
```

There are eight fixed 8 KiB windows beginning at `NX_PHYS_BASE` (0x600000000000).
Slot is 0–7; pages is 0–2 in 4096-byte units; physical is a page-aligned physical
byte address below the kernel's 1 GiB identity-mapping limit. The shared constants
are in `include/nexix/phys.h`. Successful calls return zero. Failure is -1/errno.
Effective UID zero is required for mapping, replacement and removal; successful
suser checks mark ASU just like V7. Non-root calls return EPERM before argument
checking. Raw register arguments are validated without narrowing slot or count.

Zero pages removes the window and ignores physical. Removing an empty window
succeeds. A window containing any ordinary ELF/heap/stack leaf is EINVAL even for
removal: the call never replaces or frees an ordinary mapping. Partial windows
have inaccessible remaining pages. Live mappings are user-readable/writable and
non-executable; permission changes cannot turn physical leaves into executable
or ordinary private leaves.

V7 selected an 8 KiB segment at segreg*8192 and expressed size/physical addresses
in 64-byte clicks. The native virtual windows are moved above ordinary images,
physical addresses use bytes, and sizes use pages. Source callers must adapt
those units; there is no silent interpretation as PDP-11 clicks. V7's implementation
allows 0–128 clicks (8 KiB), despite the manual diagnostics saying 0–127. The native
maximum retains the implementation's full 8 KiB capacity. This is a native source
interface, not PDP-11 binary compatibility.

## Physical range and ownership

Two backing classes are supported:

* Eligible free allocator RAM can be claimed at its specific physical address.
  A new claim is zeroed before exposure, excluding bytes left by a former owner.
  Claimed frames remain allocated until the last physical-mapping reference ends.
* The legacy VGA text-memory aperture, 0xb8000–0xbffff, is allowed device memory.
  Kernel and user aliases use matching uncached PTE attributes. VGA frames are
  never allocated, cleared or freed by the RAM allocator.

Existing registered physical frames can be mapped again by root to create aliases
and shared mappings. Other allocated RAM (kernel/page tables/ordinary process
pages) is EBUSY. Reserved/non-eligible RAM, BIOS/ROM, holes and MMIO outside the
explicit VGA aperture are ENXIO. The implementation does not offer arbitrary
kernel-memory access or infer ownership from an address merely being below 1 GiB.
Future device apertures need an explicit ownership/cache-policy integration.

Malformed slot/count, null or unaligned nonempty physical address, out-of-limit
ranges and ordinary-window collisions are EINVAL. The system-wide registry holds
128 distinct frames (NX_PHYS_FRAMES); exhaustion is ENFILE. Alias and inherited
references reuse an entry; reference saturation or page-table allocation failure
is ENOMEM. Removing the last reference releases its RAM claim; a later new claim
of the same RAM is zeroed again. Registry slots and allocator counters are retained
correctly across failed operations.

## Atomic updates and lifetime

All operations run with interrupts disabled on the single CPU. The manager first
validates the entire target window, acquires all new frame references, and maps
missing leaves before changing old leaves. Existing physical leaves can then be
rebound without allocation, and trailing old leaves removed. Failure before
commit releases new references and new tables/leaves while preserving every old
mapping and its contents. TLB entries are invalidated on replacement/removal.
Zeroing newly claimed, otherwise unowned free RAM is not rolled back.

PTE software metadata marks physical leaves as VM_PHYS. Ordinary VM mapping APIs
retain exclusive private ownership; their teardown paths distinguish physical
references from private frames. Fork eagerly copies ordinary pages but inherits
physical leaves as shared mappings of the same frames. Child exit, failed fork
cleanup, exec staging cleanup and process teardown drop exactly their references.
A process losing root may keep using inherited/existing mapped memory but cannot
change mappings, matching the distinction between access and the privileged call.

Successful exec stages physical mappings into the new address space and preserves
sharing. An overlapping new ELF/stack leaf makes exec fail EINVAL; old image and
mappings remain intact. Any other staging failure likewise leaves the caller's
image/mappings unchanged. This explicit no-overwrite rule replaces historical
segment-layout interactions. Physical pages contribute to process mapped-byte
accounting, but are not ordinary brk-owned storage. Growth cannot overwrite them.

Core dumps omit physical/device mappings, retaining their owned data/stack and
register format. This prevents device reads or shared physical bytes from being
mistaken for private crash-image storage. Snapshots and ordinary file/core records
retain their existing public versions; no private pointers are added to them.

## Verification

`make test-phys` runs host ownership/reference/zeroing/capacity/error fixtures,
QEMU acceptance at 16/128 MiB, and inventory, header and public-export checks.
Native cases verify real RW/NX page permissions, alias visibility, TLB-sensitive
replacement, VGA save/write/read/restore, denied ranges/privilege, mapping removal,
page-table OOM rollback, fork/exec sharing, last-reference cleanup and re-zeroing,
a partial exec-copy collision with a valid ELF image, and a real NX fault whose
core excludes physical leaves. Host fixtures force
partial two-page replacement and exec-copy failures and verify cleanup of staged
references without destroying old mappings. Allocator host tests cover claiming
specific free frames and rejecting allocated/reserved/unaligned frames.

Private root-test gates only locate a free RAM pair and inspect test translations;
the existing page-exhaustion gate supplies OOM. They are inactive during normal
boot. Production syscall 52 has no allocation-address discovery or test-control
arguments. `make test-unit` includes the host baseline; `make test-paging`,
`make test-shell` and `make test-storage` check protection and normal RAM/IDE use.

Task 69 supplies a separate [privileged memory-device interface](memory-devices.md)
for mem/kmem examination and patching. It does not change phys frame ownership
or permit phys mappings of kernel/ordinary process frames.

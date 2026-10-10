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

The serial transcripts are retained in `build/boot.log`, `build/ud.log`, and
`build/pf.log`, plus `build/memory-16M.log` and `build/memory-128M.log`.
Test images are separate from the ordinary boot image.
Additional paging logs are `build/wp.log`, `build/nx.log`, `build/unmap.log`,
`build/null.log`, `build/protect.log`, and `build/paging-oom.log`. `make TOOLCHAIN_PREFIX= test-paging`
runs the protection and allocation-failure cases separately.
The host parser/allocator tests also pass AddressSanitizer and UBSan with leak
detection disabled (LeakSanitizer cannot run under this session's ptrace).
This suite does not validate user-mode transitions, filesystems, or V7 execution;
those subsystems are not implemented yet.

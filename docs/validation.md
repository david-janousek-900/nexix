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

The serial transcripts are retained in `build/boot.log`, `build/ud.log`, and
`build/pf.log`, plus `build/memory-16M.log` and `build/memory-128M.log`.
Test images are separate from the ordinary boot image.
The host parser/allocator tests also pass AddressSanitizer and UBSan with leak
detection disabled (LeakSanitizer cannot run under this session's ptrace).
This suite does not validate user-mode transitions, filesystems, or V7 execution;
those subsystems are not implemented yet.

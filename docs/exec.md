# ELF64 execution (step 10)

Nexix loads native static AMD64 ELF64 executables into private user address
spaces. `kernel/elf.c` accepts a bounded byte buffer independently of pathname
lookup. `kernel/v7/exec.c` implements V7 exec replacement semantics using this
loader. Original PDP-11 a.out binaries are not accepted.

## Image format

Supported images are little-endian ELF64, System V OS ABI/version zero,
ET_EXEC, EM_X86_64, ELF version 1. Up to 32 standard-size program headers and
8 MiB of mapped load segments are accepted. Section headers are not needed.
PT_LOAD data must lie within the supplied file; filesz cannot exceed memsz.
Address arithmetic, page alignment congruence, and power-of-two p_align are
validated before allocation. Nonempty load segments must lie above USER_BASE
and below the reserved stack/guard region. Segments sharing any mapped page
are rejected. Entry must lie in file-backed bytes of an executable segment.

Load segments require PF_R. PF_W creates writable NX pages; PF_X creates
read-only executable pages. W+X is rejected. Pages are cleared before copying
file bytes, including BSS, segment tails and alignment prefixes. The loader
rejects PT_INTERP, PT_DYNAMIC, PT_TLS and executable PT_GNU_STACK. PIE,
relocation processing, shared libraries and demand paging are deferred.

## Startup stack

The stack is eight writable NX pages ending at `USER_LIMIT - PAGE_SIZE`.
Both the page above it and the page immediately below it are unmapped.
Initial RSP is aligned to 16 bytes and points to these 64-bit words:

```
argc
argv[0] ... argv[argc-1]
NULL
envp[0] ... envp[envc-1]
NULL
AT_NULL (0), value (0)
```

Strings are copied above this vector into the same stack. There are no other
auxiliary-vector entries yet. The total NUL-terminated argument/environment
strings are limited to 8192 bytes, with at most 64 arguments and 64 environment
entries. NULL argv or envp means an empty vector. Empty argc is accepted.
The initial register frame has zero GPRs, RIP at e_entry, user selectors, and
RFLAGS=0x202 (interrupts enabled, direction flag clear). Step 11 libc startup consumes this layout; see [libc.md](libc.md).

## Syscalls and replacement

V7-numbered `exec` (11) takes pathname and argv; it starts with an empty
environment. `execve` (59) takes pathname, argv and envp. All pointers and
pointer-vector entries are native 64-bit user addresses. A successful call
returns to the new entry point and never resumes the old instruction stream.
A failed call returns negative errno through the existing syscall ABI.

Pathnames are currently resolved through the embedded catalog in
`kernel/v7/catalog.c`. It includes acceptance fixtures, `/etc/init`, `/bin/sh`
and the seven rebuilt V7 utilities; see [userland.md](userland.md).
Other paths return ENOENT. Filesystem lookup will replace this catalog when
production filesystem-backed exec is integrated; no filesystem or permission checks are
claimed here. Path capacity is 256 bytes including NUL. Invalid addresses
return EFAULT; unterminated paths or oversized argument sets return E2BIG;
invalid/unsupported ELF files return ENOEXEC; allocation failure returns ENOMEM.
Managed file and cwd references survive exec. Unsupported references and
shared-text references return EBUSY; see [files.md](files.md).

Arguments are copied from the old VM into bounded kernel scratch space. A new
VM, all segments, and the complete stack are built before changing the live
process. Any failure releases staged frames and tables, leaving the old VM,
register frame and process metadata intact. Successful replacement activates
the new CR3 before releasing the old VM. PID, parent, credentials, mask and
accounting remain intact. Successful replacement closes only descriptors marked
close-on-exec; failed exec preserves all descriptors. Caught signal dispositions reset; ignored ones
remain. Profiling is disabled and AFORK cleared. Segment sizes, executable
metadata and command name are updated. No set-ID behavior exists yet.

The current single-CPU implementation keeps interrupts disabled throughout
argument staging and loading, so its shared scratch buffer cannot be used
concurrently. A future preemptible loader will need per-process scratch storage.

## Validation

Run `make TOOLCHAIN_PREFIX= test-exec` for the 16 MiB QEMU case, or
`make TOOLCHAIN_PREFIX= smoke` for the 128 MiB case. Every successful boot must
print `NEXIX_ELF_OK`, `NEXIX_EXEC_ROLLBACK_OK`, `NEXIX_EXEC_OK`, then
`NEXIX_BOOT_OK`. `make -j3 TOOLCHAIN_PREFIX= test` includes the full regression
suite. The executable is also available as `build/exec-test.elf` for inspection
with `readelf -h -l`; it requires Nexix syscalls and cannot run on the host.

Loader tests reject bad magic/class/endianness/machine/type, truncated headers,
overflowing file offsets and segment ranges, excessive header counts, invalid
entry addresses, interpreter segments, W+X, alignment mismatch, overlapping
pages and excessive image size. They inspect page permissions, stack guards,
startup vectors, empty/full vectors, unaligned segment contents, and allocation
rollback after partial
construction. The real CPL3 executable checks argv/envp, errors followed by
continued old-image execution, fork/exec/wait, freshly zeroed BSS, reset DF,
preserved mask, both exec variants, argument-count/string-size limits, and exit
status. The tests recover every allocated page and
process slot. These are execution tests for a native assembly fixture; rebuilt
V7 init, shell and utilities remain later milestones. Libc startup and initial
wrappers are implemented in step 11.

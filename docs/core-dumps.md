# Native core dumps (task 52)

Default termination for V7's SIGQUIT, SIGILL, SIGTRAP, SIGIOT/SIGABRT, SIGEMT,
SIGFPE, SIGBUS, SIGSEGV and SIGSYS attempts to write `core` in the process's
current directory. A complete dump sets bit 0200 in the wait status;
`WCOREDUMP(status)` exposes it alongside WIFSIGNALED/WTERMSIG. Other fatal
signals, caught handlers and ignored signals do not produce a core.

The reference is `usr/sys/sys/sig.c`, `psig`/`core`, with filesystem access and
inode operations from fio/alloc/rdwri, plus `usr/include/core.h` and
`usr/man/man5/core.5`. The preserved sources remain unchanged.

## File and credential rules

Nexix applies normal directory lookup/search/write permissions, creates missing
files as `0666 & ~umask` under the current UID/GID, and requires an existing
target to be a writable regular file. Existing mode/ownership are preserved.
Active executable text rejects output with ETXTBSY, including when `core` is a
hard link to the dying executable. Read-only mounts and filesystem failures
suppress the core bit. The internal writer holds an inode directly and does not
consume a user descriptor; a full descriptor table therefore does not prevent
a dump. The hold is released before normal process/text/VM teardown.

Both effective UID and effective GID must equal their real counterparts.
Eligibility is checked before creating or truncating a file. An existing core
remains untouched when set-ID credentials make the process ineligible.

Two intentional corrections to the supplied V7 implementation are explicit:

- V7 checks only effective versus real UID. Nexix also checks GID, preventing
  a set-GID process from disclosing data obtained with its effective group.
- V7 can create a file before rejecting a UID mismatch, and can report success
  because `u_error` remains zero even when its guarded body did not run. Nexix
  reports success only after all output and final inode release succeed.

The rule concerns current credentials, as in V7; there is no modern persistent
"ever privileged" or saved-ID dumpability flag. A process that fully returns
to its real IDs is eligible again.

Output truncation and writes use the existing synchronous filesystem. An I/O
failure or ENOSPC can leave a partial `core` and destroy the previous contents;
the core bit remains clear. This is not a crash-atomic file replacement or a
journal. The existing poisoned-mount policy still applies to device write errors.

## ELF64 format, version 1

The native `<core.h>` exports the format declarations from
`include/nexix/core.h`. It replaces PDP-11 machine offsets rather than defining
misleading TXTRNDSIZ/stackbas macros. Adb's native readers remain task 258.

Files are little-endian x86_64 ELF64 ET_CORE. One PT_NOTE is followed by up to
64 PT_LOAD ranges. Adjacent mapped pages with identical permissions coalesce;
holes remain holes rather than being read through. Load offsets and addresses
are page-aligned, file/memory sizes match, physical addresses are zero, and
permissions describe the original user pages. ELF executable pages are omitted,
matching V7 pure-text exclusion; owned data, heap and stack mappings are captured.
Task 64 physical/device leaves are omitted.
No kernel mappings or raw kernel `struct user`/process/file pointers are dumped.

All notes use owner `NEXIX\0`, four-byte name/descriptor padding, and these
vendor types. Readers must copy/decode note bytes rather than assume natural
alignment of a descriptor inside the file.

| Type | Descriptor |
|---|---|
| 1 / NX_CORE_INFO | 48-byte version/signal/PID/parent/real-effective IDs/time/command record |
| 2 / NX_CORE_REGISTERS | 176 bytes: fifteen general registers, vector/error, RIP/CS/RFLAGS/RSP/SS, in native interrupt-frame order |
| 3 / NX_CORE_FPU | 512-byte FXSAVE64 image, including x87/MXCSR/XMM state; reserved bytes are zeroed |

Version 1 is in NX_CORE_INFO. The ELF envelope is conventional, but these are
Nexix notes, not a claim of Linux NT_PRSTATUS or current GDB compatibility.
`readelf -h -l -n core` can inspect the envelope and vendor notes. Native adb
support will use this published layout and the original executable for code
and symbols. The dumper preserves register values from the actual fatal frame;
an invalid user stack pointer does not cause an unsafe kernel read.

The current single-CPU implementation serializes the snapshot and filesystem
output with interrupts disabled, as existing exec/filesystem operations do.
It allocates no additional VM pages or dynamic kernel buffers. A bounded segment
table overflow prevents output rather than truncating the mapping list silently.

## Acceptance

Run `make test-core`; run `make test-shell` for the interactive behavior.

- Real native page fault at 16/128 MiB: exact RIP/R12/RAX/vector, XMM pattern,
  process identity, user data/heap/stack sentinels, mapping bounds and text
  exclusion. All nine default core signals and a full descriptor table are tested.
- Real filesystem cases: umask/mode/ownership, existing directory or unwritable
  target, read-only volume, ENOSPC, active-text aliases, and preserved output
  for actual set-UID, set-GID and root-elevating executables. An ordinary exec
  with equal real/effective IDs produces a dump with the expected identity.
- Host fixtures: segment capacity, early/late writer failure, bad translation,
  close failure and credential rejection. A separate Python decoder checks
  produced bytes and invokes readelf on `build/core-fixture.elf`.
- Native shell checks on disposable RAM/IDE roots at 16/128 MiB now expect
  `Quit - core dumped`, verify the file exists and remove it.
- Existing signal, set-ID/text rollback, resource cleanup, inventory, isolated
  headers/ABI, public exports and host unit regressions remain enabled.

Acceptance probes and mount toggles are limited to self-tests. They do not
install test executables into the normal root or modify the working IDE disk.

Task 64 adds [owned physical mappings](physical-mappings.md). Physical leaves
retain shared frame references across fork/exec and are released separately from
private pages; core dumps omit those physical/device regions.

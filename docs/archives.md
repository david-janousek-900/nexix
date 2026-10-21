# Native archive tools

Task 179 installs `/bin/ar`. The V7 command interface is implemented over an
explicit native archive layer (`user/toolchain/archive.c`), replacing the original
PDP-11 structure overlays and temporary-file logic. Reference sources stay intact.
No external tool runs inside the guest.

The command supports `d`, `r`, `q`, `t`, `p`, `m`, `x`, V7 `v`, `u`, `a`, `b`, `i`,
`c`, `l` modifiers and an optional leading dash. `r` without files updates readable
members from the current directory. New members can be positioned relative to an
existing member; replacement members retain their positions. `q` preserves duplicate
names. `s` additionally writes a GNU symbol index; `S` suppresses it. Existing indexes
are rebuilt after mutation so symbol offsets cannot become stale. Indexes include
external defined/common/weak symbols and exclude undefined and local symbols.

Readers accept native SysV/GNU and BSD extended-name containers and explicit V7
0177545 and older 0177555 records. Dates and sizes use their documented PDP byte
order. Writers use GNU/SysV archives, including the long-name table and big-endian
index offsets. Historical member bytes are copied without translating object code.
Member names are basenames; path components, empty names and traversal names fail.
Guest extraction remains subject to V7's fourteen-byte filesystem component limit.
Verbose table output prints numeric mode, uid/gid, size and timestamp.

The reader validates complete inputs before mutation. Archives are limited to
32 MiB, 4096 members and 100,000 indexed definitions; allocation failure is reported.
A complete temporary archive is written and closed in the destination directory.
V7 lacks rename, so replacement uses same-directory hard links with an original
backup and rollback on link failure. HUP/INT/QUIT/TERM are ignored during writing.
An uncatchable interruption can leave a `.nxb*` recovery link; a power-fail atomic
rename is not available. Replacement changes the archive inode; other hard links
retain the old archive. `l` is accepted for V7 compatibility; safe replacement always
uses the destination directory. Existing archive mode is preserved; new archives
and extracted files honor umask. Extraction preserves ordinary mode bits and does
not install set-id permissions.

`make test-ar` runs the production code with ASan/UBSan, GNU archive interoperability,
an independently linked/executed C program, explicit V6/V7/BSD fixtures, all main
operations, long names, odd/empty members, malformed input and failed-update byte
preservation. Installed tests run at 16 and 128 MiB, including `nm` consumption and
ordinary-user access on disposable roots. Native `ranlib` and assembler/linker acceptance remain separate roadmap tasks.

## Historical conversion

Task 180 installs `/bin/arcv`. It converts either historical magic (0177555 or
0177545) to the native GNU/SysV container in place, using the checked reader and
replacement procedure above. It preserves payload bytes, names and available
metadata, including PDP-endian dates. V6 records only contain a byte of mode bits;
the converter preserves those available bits and uses the historical default gid
of 1. It cannot recover metadata absent from the old format. Native/BSD containers
are rejected as already converted, and malformed inputs remain unchanged.
Multiple input files are processed even if an earlier file fails; any failure
produces a nonzero final status.

`make test-arcv` checks independently constructed V6/V7 records, metadata and payload
preservation, truncation rejection and mixed-success input lists under sanitizers.
Guest tests at 16/128 MiB compare converted bytes against the independently checked
host result and consume a converted ELF payload with installed `nm`.

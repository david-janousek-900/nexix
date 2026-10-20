# Traditional vi acceptance (task 385)

The tested configuration is the pinned Traditional Vi 050325 single-byte BSD
editor, built with native Nexix headers/libc, historical regex, the release pool
allocator, native termcap and the checked V7 terminal adapter. It installs vi,
ex, view, expreserve and exrecover; clear is the additional task 386 command.
The preserved reference trees remain unchanged.

`make test-vi-acceptance` passed on 2026-10-20. The target is also a prerequisite of `make test`.
All guest tests create disposable disks; build/nexix.img is not a test target.
`make build`, package checks and the final inventory checks also passed; the
existing working disk retained its pre-suite SHA-256 hash.

| Check | Evidence |
| --- | --- |
| Source provenance and immutable release | test-vi-reference |
| Bounded termcap lookup, packed two-character IDs, inheritance/escapes and cursor/output routines | test-termcap, test-termcap-output, test-termcap-guest |
| VGA cursor/erase/standout and fragmented output | test-vga-controls, host guard-cell tests and real framebuffer reads |
| Native terminal validation, geometry, interrupted reads and restoration | test-termios |
| Persistent editor signal handlers, native error/directory adapters and LP64 formatter boundaries | test-vi-native |
| 6,000-line / 528 KiB buffer, temporary collision/0600 ownership, ENOSPC cleanup and unchanged saved file | test-vi-storage |
| Append, substitution, global deletion, file read/write/reopen, shell escape and failure statuses | test-ex |
| Installed full-screen vi/view, VGA/PS2 plus serial insert/motion/delete/undo/search/save, read-only rejection, >1 KiB capability strings and Ctrl-C cleanup | test-vi |
| SIGHUP preservation as uid 100, 0600 ownership, cold-boot vi -r listing/recovery, wrong-user denial and malformed input | test-vi-recovery |
| Installed aliases, helper privilege metadata and pinned manuals/licenses | test-vi-package |
| Concurrent VGA/serial editors, independent display/input/files and restored terminals | test-vi-multiuser |
| Default clear, exact ANSI serial bytes, real cleared VGA cells and errors | test-clear |
| Existing keyboard/login/password/session regressions | test-vga |
| Offline backup, new editor/helper/clear installation, configuration/data/link preservation and two cold boots | test-upgrade |
| Public interfaces, native source coverage, inventory and task mappings | test-libc-interfaces, test-inventory |

Guest checks run at both 16 and 128 MiB. The 16 MiB multi-user fixture runs two
concurrent editors (VGA and COM1) with a third serial login active. At 128 MiB it
runs editors on all three terminals. A third simultaneous editor at 16 MiB can
fail exec with ENOMEM: this resident kernel has no swap/shared physical text
pages, and its existing tables/boot image leave a small physical-memory budget.

The final gate found and corrected a termcap integration issue: BSD consumers
pass pointers into packed capability-name strings, so IDs are the first two
bytes rather than exactly two characters followed by NUL. Full-screen tests now
explicitly reject the editor's open-mode fallback. The editor's capability-string
storage also matches the full 2048-byte native termcap limit; a >1 KiB
capability entry is exercised during full-screen startup/edit/save. They also found the physical
memory limit above; the report does not claim three-editor acceptance at 16 MiB.

Limits remain explicit: single-byte text, fixed 25x80 VGA / 24x80 serial geometry,
no POSIX job-control/resize signals, and the checked termios subset documented in
termios.md. V7 directory semantics remain unchanged. If /bin/mail is absent,
preservation skips its optional mail notification. The broader complete V7 port
and release licensing/packaging gates remain separate roadmap work.

Individual task commits before final acceptance:

| Task | Commit |
| --- | --- |
| 374 | 9a2d4c8 |
| 375 | ea16dc4 |
| 376 | b07e529 |
| 377 | 807e1e9 |
| 378 | 9b5e849 |
| 379 | 0c9a065 |
| 380 | ca8e36e |
| 381 | e63f8c8 |
| 382 | dbdd85f |
| 383 | cbe103f |
| 384 | 8da9cfc |
| 386 (clear) | fdb59cf |

Task 385's commit is recorded in Git history with this report and the combined
acceptance target. Runtime logs are build/vi-*, build/ex-*, build/termios-*,
build/clear-* and build/upgrade-*; host build artifacts and logs are not committed.

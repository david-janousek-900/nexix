# Nexix manual pages

All 307 V7 manual distribution files (sections 0–8) are installed unchanged
under `/usr/man`. Traditional vi/ex/view pages are additional section 1 pages.

These are historical reference manuals. Presence of a manual does not mean its
program or every documented interface has been ported. Consult `docs/tasks.md`
and `docs/v7-userland-manifest.json` for native implementation status. Nexix uses
x86_64 ELF executables, native 64-bit interfaces and native device drivers rather
than PDP-11 executable formats and hardware. Networking is planned for 0.2.0.
Native file times retain V7 atime/mtime/ctime semantics; there is no birth time.
The kernel initializes epoch time from RTC; libc applies the configured timezone.

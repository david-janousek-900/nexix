# Traditional BSD vi port

The selected source is Traditional Vi release 050325, derived from BSD ex/vi
3.7 and 2.11BSD termcap. Its immutable imported bytes and file hashes are under
reference/ex-vi; active adaptations belong under user/vi. The V7 snapshot is
unchanged. This is an additional 0.1.0 component, with tasks 374–385.

Initial scope is single-byte vi/ex/view on VGA/PS/2 and serial terminals.
Multibyte/UTF-8, locale message catalogs and System V regex extensions are
excluded from the initial configuration; the supplied historical regex path
remains available. No external curses or Berkeley DB library is required: the
release includes libterm, historical regex and its own editor line storage.

The actual release uses termios, not sgtty directly. A checked native adapter
must map the supported terminal operations to Nexix; unsupported flags cannot
silently succeed. VGA currently needs screen-control parsing, and both terminal
families need appropriate termcap entries, geometry and TERM defaults.

Source audit/build choices: retain LISPCODE, CHDIR, FASTTAG, UCVISUAL and BIT8;
select ISO8859_1 without MB/UXRE and retain the historical regex engine. Use the
supplied bounded-pool allocator rather than mapmalloc (which requires mmap),
and account for the editor's direct sbrk use. Native pointer/line widths, signal
mask handling, file/directory APIs, preservation helpers and filesystem filename
limits need explicit acceptance. The source requires POSIX.1-1990 functionality;
current Nexix libc does not provide that entire API.

Each task is implemented, tested and committed separately. Final acceptance
requires editing/save/reopen, single-level undo, search, ex commands, view mode,
interrupt cleanup, storage failures and private preservation/recovery, using real
VGA and serial guests at 16/128 MiB. All storage tests use disposable images.

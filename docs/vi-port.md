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
maps supported operations to Nexix and rejects unsupported settings. VGA has
bounded screen-control parsing; both terminal families have termcap entries,
fixed geometry and TERM defaults. See [the adapter contract](termios.md).

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

Task 386 supplies the requested clear command using the same terminal capabilities.

Task 375 supplies native termcap lookup in libc with 2048-byte entry buffers,
a bounded sixteen-entry inheritance expansion, child overrides/cancellation,
continuations and escapes. The standard tgetstr interface requires caller-sized
string storage; reserve 2048 bytes for one complete set of distinct capabilities.
Malformed/oversize entries and numeric overflow fail explicitly.

Task 376 adds tgoto/tputs and the standard UP/BC/PC/ospeed globals. Cursor
expansion supports BSD d/2/3/./+/r/i/n/>/B/D/% operations with a bounded native
result and correction buffer. Invalid formats/coordinates return OOPS with
errno; excessive padding returns an error before output. Callback failures are
propagated. Unsupported output speed selectors retain the BSD no-padding policy.


## Installed use

Build with `make build`, then boot with `make run`. Log in and run `vi file`,
`ex file`, `view file` or `clear`. In vi, press `i` to insert, Escape to return to
command mode, `:wq` to save/quit or `:q!` to discard. Use `/pattern` to search and
`u` for single-level undo. View rejects normal writes; an explicit `:w!` has the
traditional force-write behavior when filesystem permissions permit it.

VGA defaults to TERM=nexix (25x80), serial to TERM=ansi (24x80). Override TERM
only for a matching terminal description. The initial build is single-byte;
modern vi extensions, Unicode/UTF-8 and resize/job-control signals are not supplied.

After an interrupted session, `vi -r` lists your saved buffers. Use
`vi -r filename`, inspect the result and save it. Preservation uses mode-0600
files in /var/preserve; expreserve is the checked setuid helper and exrecover
runs without privilege. If mail is not installed, recovery works without the
optional mail notification. See [native port notes](../user/vi/README.md).
Manual sources are installed in /usr/man/man1 and license/port notes in
/usr/doc/vi. The repository's V7 man/formatter port remains separate work.

An existing build/nexix.img is preserved by normal builds. Stop QEMU, run
`make upgrade-preview`, then `make upgrade-disk` to install the new files with
an offline backup. The upgrader preserves existing nonexecutable configuration,
including /etc/termcap; merge new terminal entries if that file was customized.
A fresh disposable disk receives all entries automatically.

## Checks

`make test-vi-native test-vi-storage test-ex test-vi test-vi-recovery` covers
native dependencies, storage limits, ex commands, visual editing and private
cold-boot recovery at 16/128 MiB. `make test-clear` checks the clear command.
`make test-upgrade` checks adding these programs to a customized existing image
and booting it twice. VGA/QMP tests need local UNIX socket access. Tests always
use disposable disks and preserve build/nexix.img.


The combined `make test-vi-acceptance` gate passed; see the
[acceptance report](vi-acceptance.md) for coverage and limits. It checks two
concurrent editors plus a third login at 16 MiB, and three concurrent editors
at 128 MiB. The 16 MiB resident kernel can reject a third editor with ENOMEM.

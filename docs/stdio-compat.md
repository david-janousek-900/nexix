# Native stdio source adapters (task 41)

V7's `_iobuf` layout and PDP-11 argument stack are not native interfaces.
Nexix retains its existing FILE record, buffering and count-returning printf
interfaces. Task 41 adds a native remote-format packet and checked source
adapters for every supplied non-libc C/header consumer of private stdio.
The original reference distribution and component implementation statuses remain
unchanged; completing these adapters does not finish cpp/refer/quiz.

## Remote formatting

The `remote` entry in `reference/v7/usr/src/libc/stdio/doprnt.s` reads an argument
pointer, loads the first word as the new format, and uses the remaining words
as its argument list. It jumps to the formatter loop; it never resumes the
outer format. Nexix `%r` retains this tail-replacement behavior using
`struct nx_printf_remote` from `<nexix/stdio.h>` rather than a memory overlay.

```c
#include <stdio.h>
#include <nexix/stdio.h>

int report(const char *format, ...)
{
    va_list arguments;
    struct nx_printf_remote remote;
    va_start(arguments, format);
    nx_printf_remote_init(&remote, format, arguments);
    int count = fprintf(stderr, "error: %r", &remote);
    nx_printf_remote_end(&remote);
    va_end(arguments);
    return count;
}
```

`report("%s: %d", "input", 7)` prints `error: input: 7`. A suffix following `%r`
in the outer format is discarded, as in V7. Ordinary arguments before `%r`
are still consumed from the outer list. Width/precision modifiers parsed on `%r`
do not pad the remote result; the remote format supplies its own formatting.

Initialize and end the packet while the variadic caller's argument storage is
still alive. Do not return a captured packet for use after that caller returns,
store it for asynchronous use, memcpy it, or pass a PDP-11 stack pointer. Each
formatting call copies the captured native list, so a packet is reusable within
its lifetime. Nested packets also replace the current remainder. End invalidates
the format pointer so accidental reuse is rejected before reading its list.

At most 16 remote switches are accepted per formatting call; cyclic/long chains,
NULL packets and NULL/ended formats fail with EINVAL. A valid packet is required:
these checks do not validate arbitrary user-memory pointers. String destinations
retain the completed prefix and terminate within their supplied capacity on
failure. Output counts and truncation follow the existing native snprintf
contract. Buffered file output/error/flush behavior is shared with ordinary
formatting. No signal/core/stdio binary layout or ABI version changes.

No literal remote-format call was found in the supplied application sources;
the original libc assembly nevertheless supplies the interface. Tests exercise
it as a source compatibility prerequisite, not a claim that a missing program
has been ported.

## Private FILE consumers

`tools/adapt_v7_stdio.py` stages native versions under `build/v7-stdio/`. Exact
pinned source fragments must occur once; changed, duplicated or remaining
private-stdio dependencies fail the adapter. `make test-stdio-compat` stages
these sources and compiles extracted adapted call fragments for native tests.

| Reference consumer | Native adaptation | Full program acceptance |
|---|---|---|
| cmd/refer/hunt5.c | fstat(fileno(f),...) instead of reading `_file` | 274 |
| cmd/refer/inv1.c | fdopen(pipe-write-fd,"w") with checked success instead of constructing a /dev/null stream and replacing `_file` | 274 |
| cmd/refer/what3.c | fdopen(pipe-read-fd,"r") instead of redirecting a fabricated /dev/null stream | 275 |
| games/quiz.c | setbuf(stdout,NULL) instead of setting `_IONBF` | 311 |
| cmd/cpp/cpp.c | Caller-owned static BUFSIZ buffer instead of external libc `_sobuf` | 178 |

fdopen transfers descriptor close ownership on success. Replacing the complete
initial stream construction avoids stale read-ahead/output state and does not
assume that closing a descriptor makes dup choose the same numeric descriptor.
The cpp buffer remains alive through all stream I/O. No `_iob`, `_iobuf`, private
flag aliases or synthetic FILE overlays are introduced.

Original libc's private stream manipulation is already replaced by native
stdio/scan implementations: string formatting/input use explicit native sinks
and scanning sources, stream reads/writes use native FILE buffering, and fclose/
freopen/seek/word I/O use their public contracts. Recompiling old libc internals
against a fake historical layout is not a supported adaptation.

## Verification

```sh
make test-stdio-compat test-unit test-inventory
```

The source checker scans the entire supplied non-libc C/header tree and requires
its private-stdio consumers to equal the five explicit adapters. Negative tests
cover source drift, duplicate fragments and new private dependencies. Generated
native fragments come from the staged source, not independent copies of the
replacement code. Their tests verify timestamp lookup, bidirectional pipe
stream ownership/EOF, static output buffering and unbuffered quiz-style output
through a child that bypasses exit flushing.

Ring-3 tests at 16/128 MiB check remote argument capture across integer/pointer/
floating register classes and stack arguments, long values, star width/precision,
reuse, nested replacement, discarded suffixes, preceding outer arguments,
truncation and size-only output, format errors, NULL/ended/cyclic packets, and
file error propagation. Existing stdio, signal, process and cleanup regressions
run in the same self-test boot. At task 41, `test-libc-interfaces` checked 194 public
function declarations: 183 exports and 11 deferred passwd/group functions.

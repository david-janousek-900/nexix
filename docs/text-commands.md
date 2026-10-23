# Native V7 text commands

## ed (task 145)

`/bin/ed` is the retained V7 line editor, not a replacement command language.
It supports line addresses/ranges/searches/marks, append/insert/change/delete,
print/list/join, global/inverse-global commands, substitutions and back references,
move/copy, substitution undo, file read/write/append, shell escapes and recovery.
For example:

```text
ed /root/.profile
1,$p
a
EXAMPLE=value
export EXAMPLE
.
w
q
```

The initial file is read into the editor, `1,$p` prints it, `a` appends lines,
a single `.` ends input, `w` saves and `q` exits. An unsaved default-mode buffer
produces `?` on the first quit; `Q` discards it explicitly. `ed - file` suppresses
byte counts and the unsaved-buffer warning, preserving the historical silent
mode. `-q` restores SIGQUIT's default disposition. Ctrl-C aborts the current
command while keeping the buffer; SIGHUP writes `ed.hup` in the working directory
before exiting. `!command` executes one command through the V7 shell.

`-x` and the `x` command retain V7 rotor-encrypted files with a hidden terminal
key prompt. Native libc DES `crypt` supplies the same key derivation without
requiring the unported external makekey executable. The rotor uses explicit
32-bit signed-seed/16-bit unsigned arithmetic for historical file compatibility;
its private temporary blocks also retain encryption in startup `-x` mode.
This is historical-format support, not modern encryption.

The native adaptation keeps line offsets separate from host pointers. Allocation
growth uses live reallocations and index-based relocation, not the old allocator's
free-before-realloc convention. Address arithmetic uses checked integers before
forming pointers, and empty back-reference/star matches make forward progress.
Temporary creation is exclusive and names are writable; startup and command
filenames are bounded to the retained 64-byte storage.

The original text limits remain: 512-byte line storage, seven-bit newline-ended
text, five capture groups, 128-byte BRE storage and a temporary backend below
255 blocks (about 127.5 KiB). Binary/high-bit text is not accepted outside encrypted
input. Command errors print `?` and an optional filename/message to stdout. A
regular command-file error seeks the command file to EOF; pipe/terminal commands
can continue. Ordinary command errors retain V7's eventual exit status 0; native
startup failures return 1. Scripts must check diagnostics rather than assume
modern ed error-status behavior.

`make test-ed-unit` runs the production source under ASan/UBSan on the host,
including buffer growth, copied/substituted ranges, zero-width BRE matches and
malformed/overlong inputs. `make test-ed` exercises installed commands at 16 and
128 MiB: edit/save/reopen, resizing, shell escapes, permissions, real terminal
keys, ciphertext from an independent historical-width DES/rotor implementation,
SIGINT and a child-process SIGHUP/ed.hup recovery fixture. The retained reference
source is unchanged. Logs are `build/ed-*`.

## grep (task 146)

`/bin/grep` ports the retained V7 source. `grep [-y] [-h] [-s] [-v]
[-b] [-l] [-c] [-n] [-e] pattern [file ...]` searches files or standard input.
Use separate option arguments. `-y` folds lowercase pattern letters into both
cases, `-h` suppresses filenames, `-s` suppresses selected-line output, `-v`
inverts selection, `-b` prints 512-byte block numbers, `-l` lists filenames,
`-c` counts and `-n` prints line numbers. `-e` ends option parsing so patterns
can begin with `-`. Status is 0 for selected lines, 1 for none, 2 for an error.

The retained basic regular expressions support anchors, dot, classes, star,
`\(...\)` captures and `\1`–`\9` backreferences. The native compiler bounds
its 256-byte bytecode and case expansion, validates closed captures and safely
handles empty backreferences. Classes retain V7's seven-bit interpretation;
non-ASCII class syntax is rejected. The V7 input contract is retained: long
lines are searched in 511-byte chunks and an unterminated final chunk is
ignored. This is not a modern extended-regex implementation. Recursive star
backtracking can be slow for pathological patterns.

`make test-grep` runs sanitizer bounds tests and boots the installed command
at 16 and 128 MiB, checking flags, multiple files, pipes and error statuses.

## tail (task 160)

`/bin/tail [+-number[lbcr]] [file]` retains V7 syntax. The default is `-10l`:
the last ten lines. `+number` starts at the one-based line, byte (`c`) or
512-byte block (`b`); `-number` selects a suffix. `-r` reverses lines from the
buffer, adding a newline to a final incomplete line; `-numberr` limits the
reversed line count. Files and standard-input pipes are supported. This V7
source has no `-f` or modern `-n` option.

The historical bounded-buffer behavior remains: line suffixes and reverse
output use 4,097 bytes; byte suffixes of seekable files can exceed that bound,
whereas pipes retain only their ring buffer. Count overflow and invalid
suffixes now fail with status 1, as do open, read, seek and write failures.
Successful output, including empty input, returns 0. The port fixes the
uninitialized pointer for `+1l`, argument-vector underflow, input seekability
detection after opening a file, and empty reverse output.

`make test-tail` checks files and pipes, byte/block positions, reverse output,
partial lines, overflow and ring boundaries with ASan/UBSan, then boots the
installed utility at 16 and 128 MiB.

## cmp (task 139)

`/bin/cmp [-s] [-l] file1 file2 [skip1 [skip2]]` compares binary bytes.
`-` as the first filename reads standard input. Default output identifies the
first differing one-based byte and line; `-l` lists every difference as a byte
position followed by both octal byte values. `-s` suppresses diagnostics.
Status is 0 for equality, 1 for different contents or premature EOF and 2 for
an argument, open or read error. Diagnostics retain their V7 stdout destination.

Skips are decimal, or octal when starting with zero. Invalid digits, negative
counts and native-long overflow are rejected with status 2 rather than wrapped
or silently ignored. Native byte and line counters are 64-bit. `make test-cmp`
checks binary data, EOF, stdin, skips and statuses with sanitizers and in
16/128 MiB guests.

## od (task 152)

`/bin/od [-odxhcb] [file] [+offset[.][b]]` dumps binary files or standard
input. The default is octal 16-bit words (`o`); `d` selects unsigned decimal
words, `x` or `h` hexadecimal words, `c` escaped characters and `b` octal bytes.
Flags can be combined. Word decoding is explicitly little-endian, independent
of C integer size. Odd final bytes retain V7's zero-padded second byte.
Identical complete rows collapse to `*`; a shorter final row is always shown.

Offsets default to octal, use `x`/`0x` for hex or a trailing `.` for decimal;
`b`/`B` multiplies by 512. Addresses use that base and retain the V7 seven-digit
field. Offset input must be seekable. Invalid formats, overflowing offsets,
failed opens/seeks and input/output errors return 1; success returns 0.

`make test-od` compares every format and combined formats against an
independent Python byte oracle, including all 256 byte values, duplicate rows,
odd lengths and offset bases, with sanitizers and 16/128 MiB guest boots.

## wc (task 165)

`/bin/wc [-lwc] [file ...]` counts lines, words and bytes, reading stdin
when no files are given. The default order is `lwc`; requested fields appear
in the specified order, including V7's seven-column fields and extra space
after the word count. Multiple files produce a total.

Word classification deliberately follows V7: printable ASCII starts a word;
space, tab and newline end it. Other controls and high bytes count as bytes
but do not independently start or end words. A final incomplete line counts
as bytes/words but not a newline. Counters are native 64-bit longs. Success
returns 0; unknown options and open/read/write errors return 1. Missing files
are reported while remaining files are processed. `make test-wc` verifies
these rules, field order and totals with sanitizers and 16/128 MiB guest boots.

## Retained text utilities (tasks 140–164)

These commands are the supplied V7 sources with native adaptations only where
the original relied on PDP-11 sizes, writable string literals, implicit `int`
pointers or fixed buffers. Each has an ASan/UBSan host fixture
(`make test-<name>-unit`) and installed-command acceptance at 16 and 128 MiB
(`make test-<name>`). Common adaptations:

- Fixed line, field and name buffers grow on demand instead of truncating,
  splitting or overrunning. A final line without a newline is processed as a
  line except where noted.
- Read errors, open failures and output write/flush failures produce a nonzero
  status; V7 often returned an undefined main result.
- Numeric options are range-checked; values that would loop forever or index
  outside tables are usage errors.

### col (task 140)

`/bin/col [-bfx]` retains V7's 256-line page buffer, reverse and half line
feeds (`ESC 7/8/9`, VT), SO/SI shift handling, backspace overstrikes and
tab compression; `-b` drops overstrikes, `-f` keeps half-line motions and
`-x` emits spaces. The 800-byte line becomes a growing buffer bounded at
1 MiB per line (`col: line too long`, status 2). The stdio buffer is static,
so the final flush no longer uses a dead stack frame. Bad options return 2.
The host fixture also compares 1,600 random motion/overstrike inputs with the
unmodified V7 algorithm.

### comm (task 141)

`/bin/comm [-123] file1 file2` prints the three V7 columns with `-` meaning
stdin. The column prefixes are writable arrays rather than modified string
literals. Lines are unbounded instead of split at 255 bytes, and bytes compare
unsigned. Missing files, `comm: illegal flag` and `comm: arg count` return 1.

### egrep (task 147)

`/bin/egrep [-bchlnsv] [-e] expr [file ...]` and `-f file` build V7's
deterministic automaton from the supplied grammar, generated by the retained
V7 yacc during the build. Alternation (`|` or newline), grouping, `*`, `+`,
`?`, `.`, classes, ranges and `^`/`$` anchors behave as in V7; status is 0 for
a match, 1 for none and 2 for syntax, open or limit errors, with the first
unopenable file ending the run as in V7. Native adaptations:

- Transition rows cover all 256 byte values (V7 indexed bytes above 127 past
  its 128-entry rows) and classes may contain any byte.
- Limits are 1000 expression nodes, 100000 positions and 512 automaton states;
  exceeding them reports `egrep: regular expression too long`. V7's state
  check allowed one write past the table.
- `-f` reads expressions from its own stream. V7 replaced stdin with the
  expression file, so `egrep -f file` could not read standard input.
- The scanner no longer steps past the end of the expression, reads an
  expression file's EOF correctly with unsigned native `char`, and the error
  routines take real pointers.
- Lines are unbounded instead of a 1024-byte ring. A final unterminated line is
  matched as if newline-terminated (so `$` applies) and printed unchanged.
- `-b` keeps V7's block numbering: the 512-byte block containing the byte
  before each line's newline.

The host fixture compares 1,200 random expressions and inputs with the V7
grammar built for the host.

### fgrep (task 148)

`/bin/fgrep [-bchlnsvx] [-e] words [file ...]` and `-f file` retain V7's
keyword automaton and its status values. Diagnostics name `fgrep` (V7 printed
`egrep` for some). Native word lists may use 20000 automaton states (V7: 6000)
before `fgrep: wordlist too large`; the bound keeps the eagerly mapped image
usable in a 16 MiB guest. A NUL input byte never follows an empty transition,
lines are unbounded, a final unterminated line is a line, and `-b` uses V7's
numbering. The host fixture compares 2,100 inputs with the V7 source.

### join (task 150)

`/bin/join [-an] [-e s] [-jn m] [-o list] [-tc] file1 file2` keeps V7's merge
and rewind algorithm (`file2` must be seekable; `file1` may be `-`). Lines and
field tables grow beyond BUFSIZ and 20 fields. A missing join or output field
is empty instead of a stale pointer, and an unterminated final line is no
longer read past its end. `-j` and `-o` field numbers must be positive and
missing `-e/-j/-o` arguments are usage errors (status 1). The variadic V7
error routine takes real pointers.

### look (task 151)

`/bin/look [-df] [-tc] string [file]` binary-searches a sorted file with V7
canonicalization; without a file it folds/dictionary-orders `/usr/dict/words`.
That dictionary is installed by task 280, so acceptance uses explicit
dictionaries. Entries and keys are unbounded. Matches or no matches return 0;
an unopenable dictionary or read/write error returns 2.

### rev (task 154)

`/bin/rev [file ...]` reverses each line. V7 broke lines at 256 bytes and
dropped an unterminated final line; lines are now unbounded and the final
line is reversed and terminated. Unopenable files return 1.

### sp (task 157)

`/bin/sp [width]` packs short lines onto tab stops (default width 80),
retaining V7's handling of empty lines. Input lines are unbounded instead of
overrunning 256 bytes. Widths are clamped to 1–10000.

### split (task 158)

`/bin/split [-n] [file [name]]` writes `n`-line pieces (default 1000) named
`xaa`, `xab`, ... or `nameaa`, .... A zero, malformed or out-of-range count is
a usage error instead of an infinite loop. The two-letter suffix limits output
to 676 files (`split: too many output files`), names beyond 97 bytes are
rejected and output close errors return 1.

### tabs (task 159)

`/bin/tabs [-n] [type]` emits the V7 tab-setting sequences for DASI 300/300s/
450, TermiNet 300, model 37 and HP terminals, and adjusts delay flags through
the native `gtty`/`stty` interface for 450 and TermiNet. Without a type it
inspects standard input's speed and mapping as V7 did; when stdin is not a
terminal it now does nothing instead of reading uninitialized modes.

### tee (task 161)

`/bin/tee [-a] [-i] file ...` copies stdin to stdout and every file (V7 was
limited to 19), appending with `-a` and ignoring SIGINT with `-i` or `-`.
V7's terminal/pipe chunking is retained. Unopenable files are reported and
skipped; a failed output stops receiving data while the rest continue. Any
open, read or write failure returns 1.

### tr (task 162)

`/bin/tr [-cds] [string1 [string2]]` retains V7 ranges, three-digit octal
escapes, complement, delete and squeeze semantics, and V7's deletion of NUL
input bytes. V7 wrote a terminator into a string literal (a native crash) and
looped forever on a trailing `-`, which is now literal. All other byte values
are translated through the full 256-entry table.

### tsort (task 163)

`/bin/tsort [file]` orders pairs as V7 did, reporting cycles to stderr as
`tsort: cycle in reverse order` followed by the members, then continuing.
Item names are unbounded instead of 50-byte `%s` scans; the lookup routine no
longer shadows libc `index`, and internal errors pass a valid second argument.
Odd data and unopenable files return 1.

### uniq (task 164)

`/bin/uniq [-udc [+n] [-n]] [input [output]]` retains V7 field (`-n`) and
character (`+n`) skips and the `%4d` count column. Lines grow past the
historical 1000-byte buffers (which overflowed), the final unterminated line
is kept and skip counts above 1,000,000 are rejected.

## ls output

Plain `ls` prints sorted names separated by spaces, ending each directory or
file-operand group with a newline. This compact default also applies to
redirected output. Use `ls -1` for one name per line. Detailed `-l`, block-count
`-s` and inode `-i` listings retain one entry per line. Multiple directories
retain their separate headings; empty directories print no name row.

`ls -l` shows permissions, link count, owner, group, size, timestamp and name.
Owner/group names come from the native account databases, with unsigned numeric
IDs as fallback. `ls -lg` retains V7's group-only long format.

`make test-ls` verifies the default and explicit formats, reverse ordering,
file operands, empty/multiple directories, redirection, owner/group names and
numeric ID fallback at 16 and 128 MiB.

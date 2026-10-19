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

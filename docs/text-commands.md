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

# V7 temporary-file naming (step 42)

`libc/mktemp.c` adapts the implementation supplied as
`reference/v7/usr/src/libc/gen/mktemp.c`; its public declaration is in
`libc/include/stdlib.h`. The inventory component `lib.libc` remains partial.
The reference source and `reference/v7/usr/man/man3/mktemp.3` are unchanged.

## Contract

The caller supplies a writable NUL-terminated template, conventionally with
six trailing X characters. Those characters are replaced by decimal PID digits
from right to left, zero-padding or retaining only low digits for shorter
suffixes. Five-X templates used by several original programs remain supported.
Native PIDs are 32-bit; no historical 16-bit truncation is applied before
extracting the template's digits.

The first candidate uses only digits. If it already exists, mktemp replaces the
first suffix character with successive letters a through y. If the numeric and
all 25 letter candidates exist, it returns the literal string `/`, leaving the
caller buffer holding the last y candidate. It never tries z. Successful
selection returns the original caller pointer. As in V7, any access(F_OK)
failure is treated as an available name, including permission or missing-parent
errors; the routine does not clear errno or replace this behavior with an
ENOENT-only policy.

There is no file creation, reservation or descriptor return. Repeated calls
with fresh templates may choose the same name until a caller creates it. The
subsequent file operation determines whether the name can actually be used.
This step does not add mkstemp, tmpfile or POSIX failure conventions.

The backward suffix scan is bounded. An all-X filename is supported without
reading before its start. An unused name with no trailing X is returned
unchanged, matching the original implementation. An occupied name with no X,
NULL or an empty template returns `/` with EINVAL instead of reading/writing
outside the caller string. These are explicit native handling of malformed
inputs; valid V7 template behavior is preserved.

## Inventory consumers

| Supplied source | Full consumer port |
|---|---|
| usr/src/cmd/ed.c | 145 |
| usr/src/cmd/diff.c | 143 |
| usr/src/cmd/m4/m4.c | 167 |
| usr/src/cmd/ptx.c | 273 |
| usr/src/cmd/pcc/code.c | 193–196 |
| usr/src/cmd/ar.c | 179 |
| usr/src/cmd/arcv.c | 180 |
| usr/src/cmd/strip.c | 190 |
| usr/src/cmd/tar/tar.c | 235 |
| usr/src/cmd/dumpdir.c | 233 |
| usr/src/cmd/restor.c | 234 |
| usr/src/cmd/mail.c | 248 |
| usr/src/cmd/ld.c | 185–187 |
| usr/src/cmd/troff/n1.c | 264–267 |
| usr/src/libI77/open.c, endfile.c | 115–118 |

Paths above are relative to `reference/v7/`. Several callers (ed, diff, m4,
ptx, strip, arcv and troff) pass string literals because the original platform
allowed those strings to be modified. Their native ports must use writable
arrays with lifetimes covering every later use of the returned filename.
Do not make native read-only ELF data writable to accommodate these callers.
Existing array-based callers also need template resets, error-handling and
file-creation fixtures when their complete programs are ported.

The f77 compiler's own function named mktemp creates compiler temporary nodes;
it is not this libc filename routine and is excluded from this consumer list.
No consumer program is marked ported merely because its libc dependency is now
available. Consumer acceptance remains with the listed roadmap steps.

## Verification

```sh
make test-mktemp test-unit test-inventory
```

Host tests use deterministic native PID/access fixtures for short, six-X and
long templates, 32-bit PID limits, all-X names, exact a–y probing/exhaustion,
access errors, malformed inputs and unchanged suffix-free names. Native tests
at 16/128 MiB verify real PID naming, absence of file creation, numeric and
letter collisions against actual V7 inodes, all-candidate exhaustion with z
unused, preservation of an existing file's data, missing-parent behavior and
complete namespace/descriptor cleanup. Existing stdio/signal/ABI/filesystem
regressions and kernel cleanup checks run in the same boot.

The public declaration/export checker records 195 function declarations at
step 42: 184 exports and 11 passwd/group functions deferred to steps 44–45.
Libc's remaining features and full consumer/development-interface acceptance
remain with their existing steps.

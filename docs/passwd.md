# V7 password database interfaces (step 44)

`libc/passwd.c` adapts the four supplied V7 implementations:

- `usr/src/libc/stdio/getpwent.c`: getpwent, setpwent, endpwent.
- `usr/src/libc/stdio/getpwnam.c`: name lookup.
- `usr/src/libc/stdio/getpwuid.c`: UID lookup.
- `usr/src/libc/stdio/getpw.c`: legacy raw-line lookup.

These paths are relative to `reference/v7/`. Their reference sources, pwd.h and
getpwent(3)/getpw(3)/passwd(5) manuals remain unchanged. The native declarations
in `libc/include/pwd.h` are now linkable; `lib.libc` remains partial. This step
reads existing account files. It does not change the packaged root account,
install additional users, implement authentication, or provide multi-user
login; those retain their existing roadmap steps.

## Iteration and lookup

The database path is `/etc/passwd`, resolved in the process's current filesystem
namespace. Fields retain V7's order: name, password, UID, GID, gecos, home,
shell. Empty password/shell fields are retained. pw_quota is zero and pw_comment
points to an empty string. IDs use the existing native int fields and atoi
prefix conversion: `12junk` becomes 12, as in the supplied getpwent source.
The parser does not apply credentials or silently truncate values to 16 bits;
credential-changing consumers must validate native UID/GID bounds.

getpwent lazily opens the file and returns the next entry. setpwent opens or
rewinds that iterator; endpwent closes it and is safe to repeat. EOF/read/open
errors return NULL. A final record without newline is accepted by getpwent,
matching fgets-based V7 behavior. The shell field retains any additional colons
following the first six separators; only the record's newline is removed.

getpwnam/getpwuid search from the beginning and close the iterator afterward,
including misses. Duplicate names/UIDs select the first matching record. NULL
name input returns NULL/EINVAL rather than dereferencing it. A subsequent
getpwent call after lookup starts again at the beginning. All results share one
static struct and record buffer, as in V7: copy any data that must survive later
calls. Closing the iterator does not invalidate the last parsed result.

## Legacy getpw

getpw(uid,buffer) returns zero on a match and copies the whole newline-stripped
record to the caller buffer; nonzero means not found/error. The caller supplies
at least BUFSIZ (512) writable bytes. It uses an independent stream, preserving
getpwent's current position. Unlike the original permanently cached stream,
the native adapter closes this private stream after each lookup to release its
stdio slot/descriptor. It preserves the final read/lookup errno across close.

The original getpw UID parser ignores nondigits in the third field, whereas
getpwent uses atoi. Thus `1x2` represents 12 for getpw and 1 for getpwent. This
distinction is retained. An overflowing legacy UID accumulator is skipped with
ERANGE instead of signed overflow. getpw requires newline termination, as the
original routine does; it does not match the final unterminated record. On
failure the native adapter leaves the caller's buffer unchanged. A NULL output
buffer returns nonzero/EINVAL.

## Bounded malformed records

Both readers consume complete logical records and retain at most 511 bytes
excluding newline. Longer records are skipped with ERANGE, with their remaining
bytes drained so a fragment cannot become another account. Structured iteration
skips records lacking six field separators with EINVAL. Legacy lookup requires
its two leading separators and a terminating UID separator; other fields remain
raw. EOF is not treated as an account, and read errors discard partial records.
These replace the original fragment/overrun behavior for malformed inputs.
Valid V7 records retain their documented field and lookup contracts.

The library does not clear errno after a successful call. EOF/miss does not
invent an ENOENT status; callers distinguish success by the pointer/result, with
open/read failures reporting their underlying errors. These interfaces are
non-reentrant and use V7-style shared storage, not POSIX reentrant extensions or
a shadow-password database.

## Verification

```sh
make test-passwd test-unit test-inventory
```

Native tests at 16/128 MiB create and remove `/etc/passwd` only in disposable
self-test RAM filesystems. They verify missing files, all fields, shared/static
results, EOF/rewind/reopen, first-match duplicates, empty fields, UID/GID 65535,
lookup reset/close semantics, independent legacy iteration, differing numeric
parsers, final unterminated records, malformed/oversized lines, repeated lookup
without descriptor exhaustion, and real permission denial after a child drops
credentials. Existing process/file/inode/page cleanup checks run afterward.

Host reader fixtures add exact 511/512-byte boundaries, injected partial-record
I/O errors, open failures, malformed separators and legacy accumulator overflow.
Both test types exercise the same native implementation. Header/record layout,
public declaration/export and reference inventory checks remain active.
The export audit now records 195 functions: 190 native exports and five group
functions still deferred to step 45. All six step-44 declarations have exports.

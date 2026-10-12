# V7 group database interfaces (task 45)

`libc/group.c` adapts the supplied `usr/src/libc/stdio/getgrent.c`, getgrnam.c
and getgrgid.c. The native grp.h declarations are now implemented: getgrent,
setgrent, endgrent, getgrnam and getgrgid. The reference sources, grp.h,
getgrent(3) and group(5) remain unchanged. Libc remains partial.

The path is `/etc/group` in the current filesystem namespace. Records contain
name, encrypted password, numeric GID and comma-delimited members. The first
three colons separate fields; subsequent colons are part of member text, as
in the supplied implementation. GID parsing uses native atoi prefix conversion,
so `15junk` becomes 15. The library does not apply credentials or truncate GIDs
to 16 bits; credential-changing consumers must validate their supported range.
Empty passwords and member lists are retained. Member names are not trimmed.

The V7 delimiter rules are preserved: `a,,b,` produces `a`, an empty member,
and `b`, with no final empty member. An initial comma creates an initial empty
member. Every returned member vector is NULL terminated. The historical
MAXGRP=100 pointer array now safely supports at most 99 members plus its
terminator; larger lists are skipped with ERANGE instead of writing beyond it.

getgrent lazily opens the file and returns successive entries. setgrent opens
or rewinds the iterator; endgrent closes it and can be repeated. A final record
without newline is accepted. getgrnam/getgrgid search from the beginning, select
the first duplicate match, and close the iterator afterward. A subsequent
getgrent starts again at the beginning. NULL name input returns NULL/EINVAL.
EOF, missing files, read errors and unsuccessful searches return NULL; successful
calls do not clear errno or manufacture a not-found error code.

Results, record strings and member pointers share static storage, as in V7.
Copy information that must survive another group call. Closing the iterator
does not invalidate the last result. Password and group interfaces have
independent buffers and iterators, so reading one does not advance or overwrite
the other. These are non-reentrant V7 interfaces, not supplementary-group or
POSIX reentrant extensions.

Reads are bounded to 511 record bytes excluding newline, matching the native
password reader's limit. Oversized records are fully consumed and skipped with
ERANGE so fragments cannot become invented groups. Records missing the three
field separators are skipped with EINVAL. Read errors discard partial records.
These are explicit native handling of malformed input; valid V7 record and
lookup behavior is preserved.

This task reads group files; it does not install new accounts or a default
group file, implement crypt/authentication, or port newgrp/login. Account
configuration remains task 94, crypt is implemented by task 46, and credential-changing programs
retain their owning tasks.

## Verification

```sh
make test-group test-unit test-inventory
```

Host fixtures verify field/member parsing, empty/consecutive/trailing delimiters,
whitespace and colon preservation, EOF/rewind, exact 99/100-member and 511/512-byte
boundaries, malformed records, open failures and injected partial-record I/O
errors. Native tests at 16/128 MiB create/remove account fixtures only in
self-test RAM filesystems. They check first-match duplicate names/GIDs, GID
65535, final unterminated records, static results after close, independent
password/group iteration, repeated lookup without descriptor exhaustion,
oversized membership/records and real permission denial in a child process.

Existing stdio, password, signal, file, inode and process cleanup regressions
run afterward. Header/record layout and source inventory checks remain active.
The public export audit records 195 function declarations with 195 exports and
zero exceptions. This covers currently declared interfaces only: remaining libc
features and full consuming-program acceptance are still on the roadmap.

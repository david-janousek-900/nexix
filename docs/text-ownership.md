# Executable text ownership (task 51)

Nexix implements the ownership and exclusion rules from V7 `text.c`,
`sys1.c`, `fio.c`, `sys2.c`, `slp.c`, `sys4.c` and the text/inode/process headers.
The reference remains unchanged. The native implementation is
`kernel/v7/text.c`, integrated with filesystem exec, file access and processes.

## Native representation and lifetime

A bounded forty-entry text table identifies executable inodes. Each entry holds
one managed inode reference and byte-sized text metadata. `x_count` and
`x_ccount` track attached resident images, including staged images, and
`p_textp` identifies the process's entry. ITEXT protects the inode rather than
a particular pathname, so hard-link aliases obey the same rules.

ELF VM pages remain private, and fork retains the agreed eager-copy behavior.
These are shared ownership records, not shared physical code mappings. PDP-11
core/swap addresses, swapping locks and swap-backed text copies are replaced
by this resident model. Inactive sticky entries retain inode ownership, not a
second cached ELF byte image; subsequent exec reads the immutable file again.
This preserves file exclusion and lifetime behavior without reproducing V7's
swap-backed executable cache or its performance characteristics.

Ownership is acquired after complete ELF/stack construction. A failed acquisition
rolls back the staged VM and retains the old image, credentials and text entry.
Successful exec transfers the new reference and releases the old one; re-exec
of the same inode increments before decrementing, avoiding premature release.
Fork increments the text count only after memory cloning succeeds. Exit releases
it before the process becomes a zombie, independently of the parent's wait.
The initial filesystem-loaded init image is also attached to its inode.

Foreign unmanaged text pointers still produce EBUSY at fork/exec; legitimate
native text references are supported. A full table with no unused entries
returns ENFILE and preserves the old process. Unlike V7's out-of-text SIGKILL,
this follows Nexix's existing recoverable, atomic loader-failure contract.

## V7 access and namespace rules

| Operation | Behavior |
|---|---|
| First pure-text exec with another open inode reference | ETXTBSY, including read-only or close-on-exec handles |
| Exec of an inode already marked ITEXT | May acquire another text reference, including when read handles exist |
| Write/read-write open, creat/truncate or access(W_OK) on active text | ETXTBSY, including for root |
| Read-only open on active text | Allowed by ordinary permission checks |
| Unlink one of multiple links to active text | Allowed |
| Unlink its last link while active | ETXTBSY |
| Last image exits or executes another inode, without sticky bit | Release text ownership |
| Last image releases a sticky inode | Retain an unused entry |
| Write, unlink or clearing sticky on an unused entry | Evict it, then apply ordinary operation rules |
| Clearing sticky while active | Keep protection until the final attached image releases it |

The first-exec reference check follows the supplied `getxfile` implementation;
it deliberately does not substitute modern writer-only ETXTBSY semantics.
Already-open writer references prevent that first exec, so no live write handle
can bypass protection after a successful load. A denied truncation leaves file
size and modification time intact.

Write exclusion precedes ordinary write-permission denial, as in V7 fio.c;
read-only-volume rejection precedes text exclusion. Native ELF/VM validation
finishes before ownership acquisition, so its validation/allocation errors can
precede a competing ownership error, unlike V7's incremental getxfile loader.

Unused sticky entries can be reclaimed under table pressure. Detaching/switching
the internal mounted-filesystem object purges unused entries and refuses live
inode references; the public mount/umount syscalls remain tasks 54–55. Clearing
the final text hold uses the existing inode reclamation path. Ptrace instruction
modification, exclusivity rules and debugger stops remain tasks 57–58.

## Acceptance

Run `make test-text`:

- Native filesystem exec at 16/128 MiB tests cold read/write handle exclusion,
  close-on-exec handle preservation on failure, active write/creat/truncate and
  access denial, hard-link identity and last-link protection.
- Synchronized native probes verify concurrent references, fork counts and
  release, same-inode re-exec, failed re-exec retaining protection, and successful
  replacement releasing the old inode while protecting the new one.
- Sticky tests distinguish retained ownership from cold entries, clearing sticky
  while active/unused, and eviction on write. Temporary files are removed.
- Host tests exercise the actual text module's zero-text path, reference counts,
  forty-entry exhaustion, unused-slot reclamation, foreign pointers, cache purge
  and inode-release errors without requiring a larger root fixture.
- Existing set-ID allocation-failure rollback, kernel/process/ELF/file regressions,
  inventory, headers/ABI and public exports remain enabled. Final page, inode,
  block, file and TTY counts verify resource reclamation.

The probes and synchronization gates operate only in disposable self-test roots;
the working persistent disk and normal installed program list are preserved.

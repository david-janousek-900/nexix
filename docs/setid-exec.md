# Set-UID/set-GID executable semantics (task 50)

Nexix now applies V7's executable credential rules to native ELF64 files loaded
through the filesystem. The reference implementation is
`reference/v7/usr/sys/sys/sys1.c`, in `getxfile`, supported by the credential
operations in `sys4.c` and the inode/user/process declarations in `usr/sys/h`.
No reference source or public syscall ABI is changed by this task.

## Credential rules

Exec first checks the pathname, file type and execute permissions using the
caller's existing effective UID/GID. Root still requires at least one execute
bit. A set-ID flag cannot grant permission to load an otherwise inaccessible
file. The loader then stages the ELF image, argument/environment strings and
stack alongside the decoded inode mode and ownership.

Only after loading succeeds does exec commit the new image and credentials:

| Executable/caller | Result |
|---|---|
| Set-UID, effective UID nonzero, not traced | Effective UID and process `p_uid` become the file owner |
| Set-UID, effective UID already zero | Effective UID remains zero, even if the file has another owner |
| Set-GID, not traced | Effective GID becomes the file group, including for root |
| Both flags, not traced | Apply the two rules independently |
| Process has V7 `STRC` | Suppress both credential changes and post SIGTRAP |
| Neither flag | Preserve current effective credentials |

Real UID and GID survive every exec. A later ordinary executable therefore
retains an effective ID acquired by a previous set-ID exec. The implementation
has no saved-ID or supplementary-group extension. Existing V7 setuid/setgid
rules allow an unprivileged caller to return to its real ID, rather than setting
an arbitrary ID merely because it equals the current effective ID. Returning
to the real UID also updates `p_uid`.

These are V7 rules, including the effective-root exception; they are not a
replacement with modern UNIX credential semantics. Exec does not sanitize the
environment, change umask, close ordinary inherited descriptors or configure
accounts. Existing close-on-exec and signal-reset behavior remains in effect.

## Atomicity and native integration

`files_load_exec` checks the original credentials, loads the image and attaches
the inode's mode/UID/GID to its staged `exec_image`. The former blanket ENOEXEC
rejection of set-ID files is removed. `proc_exec` applies those values only
after all fallible argument, ELF and stack construction has completed.

The single-CPU loader keeps interrupts disabled through staging and commit;
another process cannot change the executable ownership during this operation
or observe an intermediate credential/image combination. Failed lookup,
permission checks, malformed ELF, invalid user arguments or memory exhaustion
leave the old real/effective IDs, process UID, VM, descriptors and handlers
intact. Staged allocations are released by the existing ELF rollback path.
[Executable-text lifetime and ETXTBSY enforcement](text-ownership.md) are
implemented separately by task 51.

Traced exec stops with SIGTRAP before executing the new image. The acceptance
fixture now uses the real task 57 `ptrace` interface and stopped-child wait
status to suppress or reinject that trap. The former STRC toggle test gate is
retired. Debugger memory changes and single-step remain task 58. Set-ID core-dump
restrictions are implemented separately by [task 52](core-dumps.md), including
effective-versus-real GID suppression.

## Acceptance

Run `make test-setid`. The existing root-exec acceptance fixture mounts a
disposable copy of the actual root image with filesystem exec enabled. It
installs a native credential probe and uses independent executable inodes:

- No flags, each flag separately, both flags and UID/GID 65535.
- Root executing a non-root-owned set-UID file, genuine non-root-to-root
  elevation, and another exec while already effective root.
- Ordinary exec after acquiring effective credentials; fork inheritance and
  return to real IDs under V7 setuid/setgid permissions.
- Effective-ID file access, real-ID access checks and consistency of `p_uid`.
- Traced suppression and the posted SIGTRAP.
- Permission denial before promotion, root exec with no execute bits, missing
  files, malformed set-ID ELF, invalid pathname/argv/envp and oversized argv.
- Forced ELF allocation failure with only three free physical pages. Test
  gates reserve and release pages around that call; credential, descriptor,
  handler and user-memory checks verify rollback. Final kernel page accounting
  catches leaked staged pages.

Both 16 MiB and 128 MiB boots verify process/file/TTY/page and filesystem
free-block/free-inode reclamation. Temporary executables and permission fixtures
are removed. The probe is not installed in the normal root distribution, and
the gates operate only during root-exec self-tests. The working IDE disk is
preserved. Inventory, isolated headers/ABI and declaration/export checks also
run through the task target; the full host unit suite remains required.

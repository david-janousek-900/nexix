# Native V7 userland (task 16)

Nexix builds the installed V7 commands listed in [README](../README.md)
as static x86_64 ELF executables using Nexix startup code and libc. Active
sources are in `user/v7`; the original `reference/v7` tree is unchanged.

Build with `make TOOLCHAIN_PREFIX= v7-userland`. Outputs are
`build/v7-init.elf`, `build/v7-sh.elf` and `build/v7-<command>.elf`.
An installed x86_64-elf toolchain can use the default prefix instead.
No host libc or dynamic loader is used.

Libc now provides buffered file streams, integer/floating formatted output, heap
allocation, qsort, atoi, execl/execvp, alarm-based sleep and timezone-aware time conversion (see [time.md](time.md)).
Formatting supports the integer and string forms used by these sources,
including historical %D/%O. Tasks 28 and 36 supply floating-point formatting and working V7 setbuf; full libc
coverage remains pending (see [libc-coverage.md](libc-coverage.md)). The allocator reuses
freed blocks without splitting or coalescing them. Native utmp records have
8-byte line/name fields and a 64-bit time field (24 bytes total); they are not
binary compatible with historical PDP-11 utmp records.

`make TOOLCHAIN_PREFIX= test-v7` boots QEMU with 16 MiB and 128 MiB RAM.
Each boot loads the init ELF, then twice executes the rebuilt shell against a
fresh writable RAM filesystem. Tests check quoting, variables, exit status,
redirection, append, pipes, loops, conditionals, command substitution, globbing,
file copying/linking/removal, listing and working directories. The native test
driver also checks stdio, formatting, allocation and epoch conversion.
Cleanup verifies pages, filesystem blocks/inodes, descriptors and TTY references.
The test is also included in `make TOOLCHAIN_PREFIX= test`.

The task 16 acceptance fixtures retain an embedded exec catalog and their
small test filesystem. Normal boot now uses the production root image and
filesystem-backed exec, runs init as PID 1 and starts multi-user login on fresh
full IDE disks (the fixed RAM recovery image retains the single-user shell);
see [booting.md](booting.md). Task 19 verifies interactive operation; see [interactive-shell.md](interactive-shell.md); persistent
IDE storage is implemented in task 20; see [storage.md](storage.md).

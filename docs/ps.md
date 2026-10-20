# Process status

`ps`, `ps ax`, `ps alx`, `ps avx`, `ps tS1` and `ps ax PID` implement V7's
selection and display modes. Optional leading dashes are accepted. `a` includes
other owners, `x` includes processes without terminals, `l` adds scheduler and
memory fields, `v` adds child CPU times, and `t` selects a terminal name or suffix.
Exit status is 1 when no processes match, and 2 for invalid syntax.

The native counterpart of reference/v7/usr/src/cmd/ps.c uses versioned process
snapshots instead of reading kernel memory, swap addresses or an executable
namelist. Process records are readable by ordinary users and contain no kernel
addresses or environment/argument memory. Other diagnostic table kinds remain
root-only. The command displays the saved executable name rather than guessing
arguments from user stacks. Long listings report memory in KiB and a WAIT
indicator in place of historical ADDR/WCHAN addresses. Archived `k`/namelist mode
is explicitly rejected; Nexix is a resident-memory x86_64 system.

`make test-ps` tests both memory sizes with independent root and ordinary-user
logins. The kernel serializer unit checks cover process states, dense records,
64-bit sizes/times, invalid buffers and permissions; `make test-kinfo` also runs
the guest process-lifecycle and serializer regressions.

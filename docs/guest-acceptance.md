# Task 86: per-component guest acceptance

`tools/guest_acceptance.py` creates a disposable IDE root from verified staging,
starts QEMU and executes a component suite. It never attaches the working disk.
Cases identify inventory component IDs; each command checks merged serial output
and the shell's actual exit status using an unpredictable marker. Raw send/expect
steps support interactive programs, control characters and alternate prompts.
One or two independent serial pipe endpoints are supported for future terminal
and login acceptance. Timeouts, guest exit, panic and excessive output fail the run.

```
make test-guest-harness
python3 tools/guest_acceptance.py tests/guest/foundation.json --report build/echo.json --component cmd.echo
```

The runner writes a JSON result with suite/installation/ISO hashes and per-case
results, plus individual terminal transcripts and QEMU diagnostics. Failed commands
produce failure reports and a nonzero host status. The temporary root and QEMU
process are cleaned up after success or failure. Selected components share one
throwaway guest so related setup and cleanup can be expressed as sequential cases.

Suite schema 1 supports `cases`, optional `terminals` and startup regex. A case has
an `id`, inventory `component`, and `steps`. Command steps contain `command`,
expected `stdout`, numeric `status` and optional terminal/prompt. Interactive steps
contain an `expect` regex and optional `send`, terminal and timeout. Output is
normalized from serial CRLF to LF; stdout/stderr share the terminal stream.

Suites that test single-user shell behavior set `single_user: true` explicitly.
The disposable image then overrides only `/etc/ttys` to empty data; verified
staging and its receipt remain untouched. Reports record the override's SHA-256.
Production image builds expose no override CLI. Multi-user suites retain the
enabled full configuration and supply their own startup/login steps. Private
component fixtures also choose single-user setup explicitly by default.

The V7 build inventories (`cmd/in`, `cmake`, `makeall`) and individual source
fixtures inform component identity and acceptance. This is native test
infrastructure, not a port of those historical build scripts. Their ports remain
352–354. Foundation acceptance covers echo, shell failure status and file I/O at
16/128 MiB. Later component tasks add their own fixtures.

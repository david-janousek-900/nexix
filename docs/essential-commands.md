# Essential V7 commands (tasks 119–138)

Active copies live in `user/v7/cmd`; the reference tree is preserved. Commands
are native ELF64 files installed by the full-image manifest, independently of
the fixed recovery root. Original syntax and statuses take precedence over
modern Unix conventions. Guest fixtures execute installed files on disposable
IDE roots at 16/128 MiB; `make test-NAME` runs each port's acceptance.
Each task has its own implementation/test commit in Git history.

## basename (119)

Retains final-component extraction, optional suffix removal, empty results for
trailing slashes and status 1 plus a newline for missing arguments. Adapted
historical two-argument puts to native puts. No modern option parser is added.

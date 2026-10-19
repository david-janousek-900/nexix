# Native V7 make (task 168)

`/bin/make` builds dependency graphs using the retained V7 2.58 engine,
macro model, suffix rules and recipe execution. It runs native ELF programs
and `/bin/sh`. It can already drive installed commands and shell scripts;
its default compiler, assembler, lex and yacc recipes require those tools'
separate roadmap ports. Host GCC/binutils still bootstrap Nexix itself.

## Basic use

A description file named `makefile`, then `Makefile`, is tried by default:

```make
MESSAGE = built
output: input
	cat input > $@
	@echo $(MESSAGE)
```

The recipe indentation above must be a literal tab. Run `make output` to copy
`input` when `output` is absent or older. A subsequent invocation leaves an
up-to-date target alone. The first ordinary target is the default. This is
the historical make language; GNU functions, pattern rules, `.PHONY`, include
directives and parallel `-j` builds are not part of it.

Multiple targets/prerequisites, `:` and independent `::` rules, inline
`; recipe` commands, backslash-newline continuation, dependency wildcards,
recursive `$(NAME)`/`${NAME}`/`$X` macros, command-line macro overrides and
suffix inference are supported. Automatic macros are `$@` (target), `$?`
(newer prerequisites), `$<` (implicit source) and `$*` (implicit stem).
`$$` passes a dollar through make expansion. Recipe prefixes `@` and `-`
suppress command echo and ignore failure respectively. Recipe text, including
`#`, is passed to the retained V7 shell; its quoting/comment behavior applies.

The special targets `.SUFFIXES`, `.DEFAULT`, `.IGNORE`, `.SILENT` and
`.PRECIOUS` retain their V7 roles. An empty `.SUFFIXES:` clears inference
rules. Native compiler names do not silently invoke host programs.

## Options and failure behavior

| Option | Behavior |
| --- | --- |
| `-f file` | Read a description; repeat for multiple files, or use `-` for stdin. |
| `-n` | Print recipes without executing, including `@` commands. |
| `-s` | Suppress command echo. |
| `-i` | Ignore recipe failures. |
| `-k` | Continue independent targets; return failure if any target fails. |
| `-S` | Stop on a recipe failure (the default). |
| `-t` | Update target timestamps instead of running recipes. |
| `-q` | Return 0 when all requested targets are current, 255 when rebuilding is needed, or 1 on an error. |
| `-r` | Omit builtin rules; put this before description options. |
| `-p`, `-d` | Print the graph/macros or dependency debugging. |

Normal success returns 0 and parse/build errors return 1. SIGINT/SIGQUIT
return 2, removing the target under construction unless `.PRECIOUS`, a `::`
rule, dry-run or touch mode protects it. The parent retains its interrupt
handler while waiting for recipes so this cleanup works at a real console.
Open, fork, exec, wait and touch failures are reported. Recursive dependency
cycles fail explicitly.

## Native adaptations and limits

The parser in `user/v7/cmd/make/parser.c` implements the retained `gram.y`
productions directly, preserving the graph actions without requiring yacc to
bootstrap make. The retained source distribution remains unchanged. The
native scanner bounds raw lines at 1,499 bytes and expanded lines at 2,499
bytes, accepts a final line without a newline and rejects embedded NULs.
It permits at most 40 targets per rule and retains the 509-slot name table.
Macro names fit 99 bytes. Dependency and macro recursion are limited to 32
levels for Nexix's 32 KiB user stack; dependency scratch buffers use the heap.
Bounds and malformed input fail with a diagnostic rather than corrupt memory.

Native `stat` and `time` replace historical layouts; timestamps use one-second
resolution. As in V7, equal timestamps are current. Since V7's engine uses zero
as its missing-file sentinel, existing epoch-zero files are represented by 1
internally. Disk timestamps are unchanged. `-t` preserves existing file bytes.
Directory wildcards read Nexix's actual 14-byte V7 directory entries; a host
adapter only supplies directory iteration to sanitizer tests.

`archive(member)` reads the portable `!<arch>` container produced by the
bootstrap binutils, including GNU/BSD extended names. `archive((symbol))`
finds a defined native ELF64 global/weak symbol. PDP-11 object records are
replaced by explicit native ELF64 section/symbol layouts and bounded member
reads. Deterministic archive members with date zero use the archive's file
mtime (or 1 for an epoch-zero archive). This supports the installed libc/libm
archives without inventing missing dependencies. Archive writing and native
compiler/linker development remain their own tasks.

## Verification

`make test-make` instruments the production engine, parser and archive adapter
with ASan/UBSan, then boots disposable installed roots at 16 and 128 MiB.
Fixtures cover timestamp rebuild/no-op decisions, epoch-zero input, automatic
and overridden macros, suffix inference, directory wildcards, multi-file/stdin
parsing, combined rules, dry-run/query/touch, ignored/stopping/continued errors,
portable archive member and ELF symbol lookup, deep dependencies and malformed
or oversized input. Guest tests also exercise Ctrl-C target cleanup,
`.PRECIOUS` protection and a non-root touch permission failure. The host-only
directory adapter is excluded from the guest executable.

# Active V7 userland sources

These are ported copies of `reference/v7/usr/src/cmd/sh/` and the selected
`reference/v7/usr/src/cmd/*.c` commands. The original reference tree remains
unchanged. Historical notices are retained; the license and archive provenance
are in [reference/README.md](../../reference/README.md) and
[Caldera-license.pdf](../../reference/Caldera-license.pdf).

The shell retains Bourne's parser, execution logic and heap allocator. Changes
adapt pointer widths and alignment, structure member access, native startup,
signal interfaces, dup2, null pointers, directory names and clock ticks.
`native.h` and `globals.c` provide native declarations and shared definitions.
The command ports use `compat.h` and Nexix libc. Pipe identity checks in cat and
bounded directory names in pwd are adapted to the native filesystem interfaces.

GNU89 compilation preserves K&R syntax and historical implicit integer types.
The shell requires unsigned characters, signed boolean flags, common symbols
and disabled strict aliasing for its tagged pointer allocator. Its historical
makefile is retained for provenance; the repository Makefile builds the port.

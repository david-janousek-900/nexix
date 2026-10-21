# Native static linker

Task 185 implements the symbol/archive resolution pass in `user/ld/resolve.c`.
The private `ld-resolve-test` consumer exercises the production library; no
installed linker or executable output is claimed until tasks 186–187.

The reference `ld.c` remains unchanged. Its `load1`/`enter`/`lookup` symbol
collection and `ldrand` repeated archive search are adapted to ELF64 AMD64,
LP64 and exact string-table names. PDP-11 record overlays, eight-byte names,
relocation word flags and overlay/segmentation tables are replaced by checked
ELF input and a bounded hash table. Executable layout and relocations belong to
186. Historical overlay layouts are superseded by the frozen native flat ABI.

Arguments are processed in order. Explicit objects are always selected; archive
members are selected only to satisfy a required undefined symbol, or to replace
a common with a real strong definition. Each archive is searched to a fixed
point, including dependencies on an earlier member; previously visited archives
are not automatically revisited. Unselected members contribute no undefined
symbols or duplicate-definition errors. `ld_require` supplies the driver's `-u`
operation. Archive indexes are optional: the shared checked archive reader visits
members in physical order, independently of potentially stale index contents.
GNU/SysV, BSD extended-name and historical V7 archive containers use the existing
object reader; members themselves must be native ELF64 relocatable objects.

Local symbols remain private to each input. Strong definitions override weak
ones and commons. Strong commons override weak definitions; common size and
alignment merge by their maxima. First weak definitions win between weak
candidates. Duplicate strong definitions fail. Undefined weak references alone
do not extract members and remain available for zero-valued relocation in 186.
All required unresolved globals fail the final check. Absolute symbols preserve
their full 64-bit value. Failed input/resolution poisons the state; callers must
free it rather than continue linking. Empty archives contribute no objects.

Inputs validate ELF header/section spans, one static symbol table, local/global
partition, string termination, null symbol, symbol binding/type, definition
bounds and common alignment. Unsupported TLS/IFUNC and extended symbol indices
fail. Limits are 4096 input members, 32768 globals/object symbols and 32 MiB total
member bytes. Inactive members are also validated; malformed unused objects do
not silently disappear from the link. The resolver uses libc file I/O, allocation,
string operations and bounded variadic diagnostics already available natively.

`make TOOLCHAIN_PREFIX= test-ld-resolve` checks production code under ASan/UBSan,
compares archive inclusion and common merging independently with GNU ld/nm,
checks archive ordering/rescanning, local/absolute/long names, weak/strong/common
precedence, forced undefineds, duplicate/unresolved errors and malformed/fuzzed
ELF. The same resolver consumes the exact fixtures as root and an ordinary user
at 16/128 MiB, including reversed-order and malformed-input failures. Task 185
commit evidence and test commands are recorded in Git history. Remaining command,
relocation, output/install and consuming-program gates stay with 186–187.

## Static executable output (task 186)

`user/ld/output.c` emits ELF64 ET_EXEC at virtual address `0x40000000`.
File-backed read-only/code sections are concatenated into RX `.text`, writable
sections into RW `.data`, and NOBITS/commons into RW `.bss`. Each input section's
natural alignment is preserved. Data starts on a distinct 4096-byte page;
PT_GNU_STACK requests a nonexecutable stack. No segment is writable/executable.
The mapped image is bounded to the loader's 8 MiB maximum. Allocated TLS, groups,
unknown section types/flags, writable executable sections and read-only NOBITS
are rejected. Notes, comments, debug and `.eh_frame` data are discarded rather
than mapped as runtime data. Merge/string sections are concatenated without
optional deduplication. Static constructors/dynamic linking are not implemented.

Relocations support R_X86_64_NONE, 64, PC32, PLT32, 32 and 32S, with explicit
RELA addends. Target/offset/symbol/entry-size validation precedes every applied
write. Narrow results must reproduce the complete modulo-64-bit result after
zero/sign extension. Unsupported relocations, REL tables, NOBITS writes and
overflows fail. Relocations into discarded debug/unwind sections are validated
structurally and discarded. Local/absolute/global/common and unresolved-weak
values use the resolution pass; required unresolved symbols fail. Entry must
resolve inside executable text. Referenced `__nx_text_end`, `etext`, `_etext`,
`edata`, `_edata`, `end` and `_end` receive native boundary values unless defined
by an input. Output includes checked local/global symbols and string tables for
nm, size, strip and debugger consumption. The same state emits repeatable bytes.
The API returns caller-owned bytes only after success; command writing and
installation remain task 187.

`make TOOLCHAIN_PREFIX= test-ld-output test-ld-resolve test-inventory` covers exact
relocation bytes against independently fixed GNU linker addresses, Linux-host
execution of a syscall-only result, signed/unsigned overflow, weak-zero pointers,
common/BSS allocation, segment permissions, malformed relocation tables and
sanitizer fuzz inputs. The private producer links a bootstrap-compiled C program
with native crt0/libc and executes it in Nexix at 16/128 MiB as root and an ordinary
user. Guests compare exact host/native output, consume installed native assembler
output and verify failure preservation. This completes layout/relocation work;
installed command/options and broader native acceptance remain task 187.

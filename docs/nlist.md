# Native executable symbols (task 61)

The V7 sources are `usr/src/libc/gen/nlist.c`, `usr/include/a.out.h`,
`usr/man/man3/nlist.3` and `usr/man/man5/a.out.5` under the preserved reference.
Nexix implements their lookup contract in `libc/nlist.c` for little-endian
x86_64 ELF64. `<a.out.h>` includes `<nlist.h>` and `<elf.h>` for source adaptation;
it does not declare PDP-11 executable headers or accept a.out files.

```
struct nlist names[] = {
    {"main", 0, 0},
    {NULL, 0, 0}
};
if (nlist("/bin/program", names) == 0) {
    /* names[0].n_type and names[0].n_value describe main, if found. */
}
```

The native record is 24 bytes: const string pointer at 0, int type at 8,
uint64_t value at 16. Names have no eight-character truncation or automatic
underscore translation. A null name pointer or an empty name terminates requests.
Following V7, only the first of duplicate requests is filled. In file scan order,
the last matching symbol wins; there is no implicit global-over-local preference. A successful call
returns 0 even when some or all symbols are missing, with their type/value zero.
A failure returns -1/errno and clears all requested type/value fields, including
results obtained before a later malformed symbol. Name pointers are retained.
Success leaves errno unspecified. Null filename/list is EINVAL; inaccessible files
retain open's error; malformed or unsupported symbol formats are ENOEXEC. I/O and
close errors are reported and the descriptor is closed on every path.

The reader accepts ET_EXEC, ET_DYN and ET_REL, scanning SHT_SYMTAB tables in file
order, or SHT_DYNSYM when no full symbol table exists. Sectionless executables and
files without symbol tables succeed with missing results. It does not decode
PT_DYNAMIC when section tables have been removed. Values in ET_DYN are unrelocated
link-time addresses and those in ET_REL are section-relative; runtime relocation
and load-bias resolution belong to consumers.

V7 N_* values are retained. Executable sections map to N_TEXT, other initialized
sections (including read-only data) to N_DATA, NOBITS to N_BSS, absolute symbols
to N_ABS and undefined symbols to N_UNDF. Common symbols retain V7's undefined
classification with the allocation size as value. Global and weak bindings add
N_EXT; local bindings do not. FILE symbols map to N_FN. SECTION and TLS symbols
are omitted: a TLS offset is not an ordinary process address. Exact richer ELF
binding/type information remains available through the shared fixed ELF64 record
contracts in `<elf.h>` for later readers; nlist's type field is intentionally V7's
coarser classification.

Header identity/version/machine/size, section/table extents, symbol record sizes,
string-table links, section indices and terminated symbol names are checked.
Bounds use subtraction to avoid addition/multiplication overflow. Reads retry
EINTR and assemble short reads; unexpected EOF is ENOEXEC. Names are compared in
128-byte chunks without file-sized allocation or a fixed name length limit.
Extended section counts and SHN_XINDEX are explicitly unsupported (ENOEXEC).
This interface reads symbol metadata, not an execution validator: program headers,
relocations and instruction validity remain the executable loader's responsibility.

`make test-nlist` runs host malformed-file/error fixtures, isolated header/ABI and
export checks, and native filesystem acceptance in QEMU at 16 and 128 MiB. Tests use an
independently encoded ELF fixture with full names and values above 32 bits, all
principal classifications, missing/duplicate requests, stripped files and malformed
tables/names, plus short reads and injected I/O failures. A native caller resolves
`main` and the entry address from the installed `/bin/echo` and exercises actual filesystem
creation, malformed-file cleanup and repeated failed opens.

Kernel diagnostic exports (task 62), toolchain ABI finalization (177), nm and
other diagnostic/debugger/profile program ports retain their separate tasks.

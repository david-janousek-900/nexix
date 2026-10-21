# Native object tools

These ports retain V7 command behavior where applicable and replace PDP-11
object/archive structure overlays with checked native format readers. ELF files
must be little-endian ELF64 AMD64; ET_REL, ET_EXEC and ET_DYN may be inspected.
Inspection of ET_DYN does not add dynamic execution support to the kernel.

Inputs are bounded to 32 MiB and must fit guest memory. GNU/SysV archive indexes
and long-name tables, BSD extended names and V7 archive containers are recognized.
The original reference inputs stay unchanged. Reading a historical container
does not translate or execute its PDP-11 object payloads. Non-ELF archive members
are skipped; malformed ELF members or archive framing fail with status 1.
Thin archives, extended ELF section numbering and SHN_XINDEX are unsupported
and rejected. Reads do not modify input files.

## nm (task 188)

`nm [-goprun] [file ...]` defaults to `a.out`. `-g` selects external symbols,
`-u` prints only undefined symbol names, `-n` sorts numerically, `-r` reverses
sorting, `-p` retains symbol-table order and `-o` prefixes each line with its
input/member name. Options may be combined or supplied separately; `--` ends
option parsing. Multiple objects and archives retain object headings unless
`-o` is selected. Errors return 1; invalid options return 2.

Addresses use sixteen hexadecimal digits and symbol names are not truncated to
eight bytes. T/D/B/A/C/U identify text, writable data, BSS, absolute, common
and undefined symbols; local variants are lowercase. ELF read-only data uses
R/r; weak function/notype and object symbols use W/w and V/v. V7-style file
metadata uses f. Anonymous and section symbols are omitted. Static symbols are
preferred over dynamic symbols when both tables exist.

`make test-nm` runs the production sources under ASan/UBSan on the host, followed
by installed guest acceptance at 16/128 MiB. Tests use actual GCC objects and
GNU ar output, independent BSD/V7 container encoders, long symbol/member names,
undefined/common/weak/read-only symbols, sorting/selection/prefix modes and
malformed/truncated input rejection. LSan is disabled because it requires
ptrace support absent from the managed host; address/undefined-behavior checks
remain enabled. A sanitizer crash is a test failure, including for bad inputs.

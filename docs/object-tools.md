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

## size (task 189)

`size [file ...]` retains V7's `text+data+bss = totalb = 0octalb` format, using
64-bit byte counts. The default is `a.out`; multiple files gain a filename
prefix and archive members use `archive(member)`. Errors return 1 and later
operands are still processed; unsupported options return 2.

ELF SHF_ALLOC sections determine sizes: nonwritable initialized sections count
as text (including read-only data), writable initialized sections as data, and
SHT_NOBITS as BSS. Nonallocated metadata is excluded. A sectionless executable
is measured from PT_LOAD file/memory sizes, separating writable initialized
bytes and zero-fill; these are segment sizes, including any segment padding,
rather than fabricated section sizes. Integer overflows and out-of-file ranges
are rejected. Symbol-table contents are unnecessary for size measurement.

`make test-size` compares real object sizes with GNU size, checks independently
encoded archive containers and a sectionless static executable, and exercises
default/multiple operands and malformed inputs. ASan/UBSan host checks and
installed guest checks at 16/128 MiB must pass. Shared reader changes also run
`make test-nm` to protect symbol inspection.

## lorder (task 202)

`/bin/lorder file ...` prints object dependency pairs: every object gets a self
pair, and each external undefined symbol creates edges to objects defining it.
It preserves repeated edges and the original script's duplicate self-pair for
a single `.o` operand. No operands print the original usage line and return 0.

The active shell wrapper is `rootfs/bin/lorder`; the original reference script
is unchanged. Its nm/sed/sort/join pipeline is implemented by the installed
native helper `/usr/lib/lorder-native`, sharing the checked ELF/archive reader
with nm and size. This removes the need to wait for unrelated text-tool ports
and avoids the original predictable temporary files. The wrapper handles V7's
empty quoted `$@` behavior explicitly and passes supplied operands quoted.

ELF global/weak definitions include BSS, common and read-only data as well as
text and initialized data. A reference to an absent definition creates no edge;
linker resolution remains a separate task. Multiple providers yield conservative
edges to each provider. Archive members use `archive(member)` identities.
Malformed/missing files return 1; whitespace in filenames is rejected with
status 2 because the historical pair protocol cannot represent it unambiguously.
The implementation is bounded to 1024 object units and 100000 external symbol
records, with normal memory-allocation failures reported as input failure.

`make test-lorder` checks host production code under ASan/UBSan against known
dependency graphs (including cycles), multiple definition kinds, single/no
operands, GNU/BSD/V7 containers, long names and malformed inputs. Installed
script/helper guest tests at 16/128 MiB check graph output, argument forwarding,
archive identity and error statuses. `make test-install-unit` additionally
checks wrapper byte provenance, real helper ELF validation and rejection of
nonexecutable or incorrectly owned helpers.

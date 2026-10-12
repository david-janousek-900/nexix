# Native V7 headers and ABI contracts (step 39)

[header-coverage.json](header-coverage.json) accounts for all 57 files supplied
under `reference/v7/usr/include`, including the assembler interfaces `sys.s` and
`sys/pk.p`. Each input has a native contract, an explicit native disk replacement,
or a deferred consumer/feature owner. It is a source adaptation map, not a claim
that all declarations have implementations. The reference headers are unchanged.

## Native data model

| Interface | Native contract | Historical difference |
|---|---|---|
| `int`, PID | Signed 32 bits | PDP-11 int was 16 bits |
| `long`, pointer, size_t, ssize_t | 64 bits | Native AMD64 LP64 |
| `off_t`, `time_t`, `clock_t` | Signed 64 bits | Historical longs were 32 bits; disk times remain 32-bit encoded |
| `dev_t`, `ino_t`, UID/GID, mode | Unsigned 16 bits | Device identity retains eight-bit major/minor; negative device sentinel is 65535 |
| `daddr_t` | Signed 32 bits | V7 block-address width retained independently of native long |
| `caddr_t` | Native char pointer | No PDP-11 address truncation |
| `stat` | 48 bytes; first eight 16-bit fields then size/atime/mtime/ctime at 16/24/32/40 | Explicit reserved halfword; no creation-time extension |
| `direct` | 16 bytes: inode at 0, 14-byte name at 2 | Historical directory records retained |
| `tms` | Four 64-bit tick fields, 32 bytes | HZ/CLK_TCK = 100; historical HZ was 60 |
| `timeb` | 16 bytes: time at 0, milliseconds at 8, timezone at 10, DST flag at 12 | Native padding; milliseconds implementation pending step 363 |
| `sgttyb`, `tchars` | 8 and 6 bytes; flags at offset 4 | Character fields represented as unsigned bytes; native 32-bit flags |
| `utmp` | 24 bytes: eight-byte line/name, 64-bit time at 16 | Native session records, not historical binary fixtures |
| `passwd` | 64 bytes; int UID/GID/quota, native string pointers | Database routines implemented in step 44 |
| `group` | 32 bytes; int GID and native member-vector pointer | Database routines implemented in step 45 |
| `jmp_buf` | Nine 64-bit words | Native preserved registers and FP controls; no six-word PDP-11 label overlay |
| Signal context | ABI version 2, 688 bytes | Native interrupt frame plus 512-byte FXSAVE64 image |

Step 39 adds missing device/inode/block/pointer typedefs, device selectors and
constructor, filesystem constants and inode address helpers, historical signal
aliases, mpx file-type constants and historical terminal flag values. Device
constructor inputs must be in 0–255; consumers must validate untrusted values.
The filesystem constants use explicit disk widths: `NINDIR` stays 128 despite
native LP64. `itod`/`itoo` accept valid nonzero 16-bit inode numbers.

All `stat` field offsets are checked against the shared kernel record in the
public header. Terminal record size/flags checks likewise protect direct
kernel/libc copying. These are additive source contracts: no existing syscall
number, record size or supported runtime behavior changes, so ABI version 2
remains valid.

## Source compatibility and deferred interfaces

`pwd.h` and `grp.h` expose all original record fields and typed native routine
declarations. Password and group database functions are implemented by steps 44–45.
Terminal flag constants do not enable TANDEM, LCASE or delay modes: those remain
step 74 and the driver continues rejecting unsupported configurations. Mpx mode
constants likewise do not implement channels (65–67).

The original `sys/types.h` includes a six-int kernel `label_t`; consumers must
use native `jmp_buf`/kernel context switching, with runtime assembly adapters
owned by step 79. Original `sys/param.h` also contains physical addresses,
click/segment conversions, static kernel table sizes and interrupt-priority
bit masks. These are private architecture implementation details and are not
copied into the public native header. Native memory/device diagnostics use
explicit interfaces from steps 62–64 instead of historical kernel offsets.

Headers for a.out/core/symbols/archives, standalone I/O, packet discipline,
arbitrary-precision arithmetic and dump/tape formats remain with their
consumers as listed in JSON. Native ELF contracts are frozen in step 177;
historical media formats need explicit encoders, not LP64 structure writes.
`varargs.h` and internal historical FILE layouts need source adapters (41),
not pointer arithmetic over PDP-11 call stacks. `assert.h` is implemented by step 40 with V7 exit(1) semantics; see
[scalar and byte helper contracts](libc-helpers.md). A header's presence never implies its pending library or
kernel feature is implemented.

Original kernel-private headers under `usr/include/sys` remain private. Their
runtime counterparts live under `kernel/include`; mpx, mount, text, packet,
profiling and accounting data follow their feature steps. The kernel's byte-array
inode/superblock records replace `sys/ino.h`, `sys/filsys.h` and `sys/fblk.h`
layout assumptions. Disk format testing remains in `tests/v7_types.c` and
filesystem tests. Userland programs needing raw filesystem formats will acquire
public explicit encoders with their consuming ports.

## Verification

`make TOOLCHAIN_PREFIX= test-header-abi` compiles every public libc header twice
in isolation in both C11 and GNU89, with `-nostdinc` and only compiler fundamental
headers plus Nexix headers. This prevents accidental success through host libc.
A compiled fixture checks independent record sizes/offsets, every 16-bit device
number, inode boundary mappings and constants. Coverage validation rejects
missing/duplicate reference inputs, invalid owners, unsafe native paths and
unknown dispositions. The target also runs through `test-inventory`.

Run `make TOOLCHAIN_PREFIX= test-unit test-libc test-v7` for existing disk-layout,
native libc and rebuilt V7 consumer regression tests. These validate the current
subset; complete development-interface installation and consumer acceptance
remain step 82, and full toolchain/distribution acceptance remains 355/359.

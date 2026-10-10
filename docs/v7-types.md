# V7 core types on x86_64 (step 7)

The headers in `kernel/include/v7/` adapt the original headers in
`reference/v7/usr/sys/h/`. The reference source remains unchanged; its provenance
and retained license are documented in `reference/README.md`.

`core.h` defines process, per-process user data, inode, open file, buffer, and
shared text structures. Historical field names remain recognizable. These are
internal kernel contracts, not a published libc/syscall ABI. They compile in the
freestanding kernel and are now used by the native process scheduler described
in [processes.md](processes.md).

Runtime offsets and time values are signed 64-bit. PIDs are signed 32-bit;
inode numbers, user/group IDs, and device IDs retain unsigned 16-bit ranges.
Device major/minor numbers remain eight bits each. Block numbers are signed
32-bit; inode disk addresses remain limited to 24 bits. Runtime reference counts
are widened to 32 bits. Memory sizes and buffer residuals use byte counts.
Syscall argument slots and signal dispositions retain full native address width.
The names are prefixed where necessary to coexist with host headers in tests.

Each process owns an address-space descriptor, a kernel stack range, a context,
and a pointer to its user data. The context reserves RBX, RBP, R12–R15, RSP and
RIP for the native switch implementation in `arch/x86_64/context.S`. Zombie
status and accounting fields are explicit rather than overlaying `struct proc`
with V7's `struct xproc`. There is no fixed-address global `u` or kernel stack
embedded in the user structure. Future code must obtain user data from the
current process.

PDP-11 segmentation registers, click addresses, separate instruction/data
spaces, FP registers, swap jump buffers, and UNIBUS extension fields are omitted.
FPU support and interrupted-syscall continuation need explicit x86_64 designs.
Executable metadata records native entry and byte sizes; ELF parsing is later.
Multiplexed channels and swapping are deferred. Preserved historical flag values
in `constants.h` describe source compatibility, not implemented capabilities.
The native scheduler accounts in 100 Hz PIT ticks; future libc exports must
reflect this rate rather than V7's original 60 Hz.

`disk.h` represents disk records using byte arrays, never native pointer-bearing
structures. Inodes occupy 64 bytes, directory entries 16 bytes, and superblocks
512 bytes (440 bytes of fields and 72 unused bytes). Compile-time assertions
check sizes and key offsets. Indirect blocks contain 128 four-byte addresses.
PDP-11 shorts are little endian; 32-bit values store their high 16-bit word first.
Three-byte inode addresses store bits 16–23, 0–7, then 8–15, matching the original
`usr/src/libc/gen/l3.c`. This selects the PDP-11 filesystem format, not the
alternative Interdata build. Encoders here handle unsigned bit patterns; later
filesystem code must validate signed sizes/times and native-to-disk ranges.

`make TOOLCHAIN_PREFIX= test-unit check` checks native layouts, known disk byte
patterns, address overflow rejection, inode-block boundaries, all major/minor
pairs, and the freestanding ELF build. `make TOOLCHAIN_PREFIX= smoke` verifies
the existing QEMU boot path. Filesystem I/O and serialization of entire records
remain step 12. Step 8 now implements process ownership and scheduling using
these structures;
[processes.md](processes.md) describes allocation, teardown, and active-process
rules.

# V7 scalar, byte and conversion helpers (step 40)

Step 40 supplies the remaining helper contracts listed in the roadmap, plus
V7 assertions. The reference source tree remains unchanged. Libc remains
partial: crypt, process-pipe, profiling,
symbol lookup and feature-specific wrappers retain their existing owners.

| Interface | V7 origin | Native contract |
|---|---|---|
| `swab` | libc/gen/swab.c | Exchange each adjacent byte pair; odd final byte stays untouched. Zero/negative counts do nothing. Unaligned and exact in-place buffers work; other overlap is not promised. |
| `ltol3`, `l3tol` | libc/gen/l3.c | Convert native eight-byte long arrays to/from V7 three-byte addresses. `0x123456` packs as `12 56 34`, independent of host byte ordering. Decoding zero-extends; encoding discards bits above 24 as the original routine did. Zero/negative counts do nothing. |
| `gcvt` | libc/stdio/gcvt.c | Retain V7's trailing-zero suppression, leading decimal point and F/E thresholds, using native ecvt. For example `.0001`, `1.e-05`, `10000`, `1.e+05`; not modern printf `%g`. |
| `abort` | libc/gen/abort.s | Deliver SIGIOT (6) through the native signal ABI. Default action terminates; caught/ignored signals allow return with zero, as the original IOT instruction followed by `clr r0` did. No forced disposition reset, stdio flush or exit callbacks. |
| `tell` | libc/gen/tell.c | Native off_t result of lseek(fd,0,SEEK_CUR); descriptor offset, not the stream's buffered logical position. Preserve EBADF/ESPIPE and full-width offsets. |
| `execle` | libc/sys/execle.s | Collect a NULL-terminated native variadic argument list and its following environment pointer, then execve. Do not change the caller's environ on failure. Use pointer-typed NULL sentinels in variadic calls. Accept 0–64 arguments; reject 65 with E2BIG before invoking exec. |
| `assert`, `_assert` | usr/include/assert.h | Evaluate enabled expressions once; write the historical file/line diagnostic and exit(1), including normal exit callbacks. NDEBUG removes expression evaluation. Re-including the header updates the macros. |

`ltol3` deliberately follows the original truncation contract; the kernel's
`v7_disk_putaddr` rejects overflow instead. Filesystem consumers must validate
addresses before storing them. Packing is not a native long memory overlay.

`gcvt` uses native binary64 and the existing half-up ecvt conversion, not a new
correctly-rounded algorithm. It clamps significant digits to 1–64 to avoid the
original invalid-count buffer indexing. The caller supplies a writable buffer
of at least 80 bytes. Three-digit exponents support the binary64 range;
inf/nan and signed zero follow native ecvt's spelling/sign extensions.

The public `abort` returns int to preserve V7's returning path. Nexix code is
compiled freestanding with compiler builtins disabled, including the host
calendar fixture; a hosted C compiler's ISO abort declaration is incompatible.
Core-dump production remains step 52. The SIGIOT default termination test does
not claim that a core file is generated.

## Declaration and export acceptance

`tools/check_libc_interfaces.py` asks GCC for declarations from all public
headers (`-aux-info`, with host libc headers excluded), then checks native
libc/libm archives with nm. Grouped declarations and function-pointer arguments
are parsed from the compiler output rather than guessed from source text.
Every function must have an export or an explicit unfinished owner in
[libc-declared-interfaces.json](libc-declared-interfaces.json). Step 40 recorded
192 declarations: 181 exports and 11 passwd/group declarations deferred to
steps 44–45. Public errno/environment/stream/ctype data exports are checked too.
The check rejects missing exports and stale or completed-owner exceptions.

An export proves link availability only. Existing partial functions such as
ftime and execvp still require their owning feature/consumer acceptance;
no stub becomes a completed feature through this check. Full libc integration
and installed development interfaces remain step 82.

## Tests

Run:

```sh
make TOOLCHAIN_PREFIX= test-libc-helpers test-stdio test-unit test-inventory
```

The libc helper target runs ring-3 tests at 16 and 128 MiB. Golden byte fixtures
cover unaligned/in-place/odd-length swapping and multiple packed addresses,
high-bit truncation, sign handling and untouched boundaries. Conversion tests
cover V7 F/E transitions, rounding carry, signed zero, IEEE special values and
three-digit/subnormal exponents. Child processes verify SIGIOT default/caught/
ignored behavior and assertion diagnostics, exit status, callbacks and NDEBUG.
Exec tests cover environment selection beyond register arguments, failed exec,
64/65-argument limits and an empty vector/environment. Pipe and invalid descriptor
tell failures are tested; stream tests verify descriptor read-ahead differs
from the stream's logical position.

The filesystem self-test's executable scratch buffer is enlarged from 64 to
128 KiB because the expanded libc test ELF exceeds 64 KiB. This changes a test
fixture, not the kernel ELF loader's size limit. Existing lifecycle and page/
file/inode cleanup checks run after the new child-process tests.

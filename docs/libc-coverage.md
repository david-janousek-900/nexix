# Native libc interface progress (steps 28 and 33–49)

Libc remains partial. The implementations below are native interfaces for
rebuilt V7 sources; their addition does not complete V7 userland or the remaining kernel/libc
prerequisites (steps 38–82). The original source distribution remains unchanged.

## Input conversion

Native scanf, fscanf, sscanf, vscanf, vfscanf and vsscanf use SysV va_list
arguments instead of the PDP-11 stack-pointer mechanism in V7 scanf.c/doscan.c.
Supported conversions are d/i/u/o/x, e/f/g, c/s, literal scan sets, assignment
suppression, widths, h/l modifiers and historical uppercase long conversions.
The native %n extension reports consumed bytes without increasing assignments.

Scanf scan sets preserve V7's literal-character treatment of '-': [a-z] matches
'a', '-' and 'z', not a character range. Negation and an initial literal ']'
are supported. Byte-valued input, including 128–255, uses a 256-entry membership
table. Strings terminate with NUL; %c does not. Callers supply buffers large
enough for the chosen widths, as in V7. Numeric input streams without a fixed
token buffer, so long fields cannot overwrite a stack array. An unmatched
conversion stops scanning; input failure before an assignment returns EOF.
Incomplete decimal exponents are consumed and rejected as matching failures.
Textual inf/nan and hexadecimal floating input remain unsupported.

Integer accumulation uses explicit unsigned bounds and saturates the selected
native destination type with ERANGE on overflow. Floating input uses binary64
decimal conversion with range reporting, including overflow/underflow when
narrowing to float. This avoids historical signed-overflow and token-buffer
bugs, but is not a claim of correctly-rounded decimal conversion or complete
C99 scanf support. Width and h/l conventions retain native LP64 sizes.

## Streams

fdopen validates the descriptor and requested access without truncating a
file. Append mode enables the descriptor's native append flag. A successful
fdopen transfers close ownership to the stream; failure leaves the descriptor
open. freopen closes the previous descriptor before opening its replacement,
including on failure, following the retained V7 implementation's ordering.
FILE's public native layout has expanded; all callers must rebuild with the
current header. It is not the historical _iobuf layout.

ungetc preserves one byte, clears EOF on success, and returns **zero on
success**, following V7 rather than the modern byte-return convention. A second
pending byte or EOF is rejected. fread consumes pending pushback before reading
the descriptor. ftell and relative fseek account for that pending byte; a
successful seek discards it. Streams lazily allocate a BUFSIZ (512-byte) buffer on first I/O. Output to
terminals and the initial stderr stream remain unbuffered, following V7.
Allocation failure falls back to unbuffered I/O. setbuf accepts a caller-owned
BUFSIZ-byte array, or NULL for unbuffered I/O; as in V7 it should be called
before I/O and the array must remain alive until the stream closes. Only
library-owned buffers are freed by fclose/freopen.

fflush writes pending output, retries interrupted writes, and retains any
unwritten bytes after an error. fflush(NULL) flushes every active output
stream (a native convenience); fflush on input leaves read-ahead and pushback
untouched, following V7 rather than a POSIX input-flush extension. fclose
attempts both flush and close and reports either failure. Normal exit flushes
after atexit handlers; _exit and successful exec do not flush userspace buffers.
fork copies pending buffers: callers must flush before fork when both parent
and child will use normal exit, to avoid duplicate output.

Read-ahead and pending output are included in ftell and relative fseek.
Successful seeks clear EOF/pushback and reset buffering direction; failed
input seeks retain unread data, including on pipes. Update-stream direction
changes flush output or seek back over unread input; a direction change which
requires seeking on a pipe fails with ESPIPE. Pending append output is counted
from the current file size without changing the descriptor offset. Read/write
access checks set the stream error indicator and EBADF.

gets retains the V7 caller-owned buffer interface and removes the newline.
getw/putw serialize a native four-byte int in little-endian order, matching
Nexix's int width. They do not decode historical two-byte PDP-11 data; consumers
of preserved binary assets need explicit disk-format adapters. A stored -1 word
is distinguished from EOF with feof/ferror.

Printf-family formatting now supports '*' width/precision, '+', space and '#'
flags, integer precision and existing left/zero padding. D/O/U/X use native long
arguments as in V7; X prints lowercase hexadecimal and is not C99's uppercase
int conversion. Floating precision remains limited to 63. Unsupported formats
return EINVAL and terminate a string destination after any completed prefix.
The historical remote %r convention is supported through native captured
va_list packets; see [stdio compatibility](stdio-compat.md).

## General helpers

ctype.h retains V7 classification bits, ASCII table entries and arithmetic
toupper/tolower macros. Callers test islower/isupper before converting, as in
the original header. The table has zero entries for high bytes and EOF; native
classification evaluates its argument once and rejects out-of-range indexes.
V7's original space/control classification quirks are retained.

strcat, strncat, index and rindex supplement the existing string routines.
strtol/strtoul support bases 2–36, auto-base and hexadecimal prefixes, endpoint
reporting and native LP64 overflow limits. atol/atoi use those conversions;
atoi additionally checks the native 32-bit int range. abs/labs and srand/rand
are provided. Rand retains V7's 15-bit sequence, using explicit 32-bit unsigned
wrap instead of signed PDP-11 long overflow.

## Acceptance and remaining work

`make TOOLCHAIN_PREFIX= test-stdio` runs native ring-3 tests at 16 MiB and 128 MiB.
They check mixed register/stack scanf arguments, suppression, widths, malformed
input, EOF, long numeric fields, scan sets, range errors, integer endpoints,
formatting flags, V7 rand values and character/string contracts. File tests
exercise pushback, logical offsets, partial elements, append mode, descriptor
ownership, failed reopen, native word I/O and redirected stdin scanf/gets.
Buffering tests additionally verify caller-buffer use, deferred output and
explicit/universal/close/exit flushes, unbuffered output, multi-buffer read/write,
read-ahead offsets, update and append streams, failed seeks on pipes, broken
pipe/descriptor errors, immediate redirected stderr, callback output, and
fork/exec/_exit behavior including inherited buffer duplication.
Existing VM, inode, block and descriptor cleanup assertions remain active.

Remaining libc prerequisites include password/group database and crypt
interfaces, process-pipe helpers (popen/pclose/system), tty/account lookup,
temporary files, complete error text, profiling startup/monitor, ELF symbol
lookup, native headers and adapters for users of internal V7 FILE fields.
Required kernel accounting, tracing/profiling, kernel-information and
mount/raw-device interfaces are also unfinished. Their wrappers must accompany
working kernel behavior; ENOSYS stubs do not complete a port.

Step 36 (previously 25.8: buffered stdio and setbuf) is implemented. This completes that
substep's stream behavior and regression coverage; the remaining kernel/libc
interfaces listed above are still pending (steps 38–82). Step 41 supplies source adapters for the five supplied non-libc private-stdio
consumers. Full program acceptance remains with their owning port steps.

Step 40 adds [V7 scalar/byte/conversion helpers](libc-helpers.md), including
returning abort, historical disk-address packing, gcvt, tell, execle and
exit-based assertions. Public declaration/export coverage is checked separately
from complete feature behavior; libc remains partial.

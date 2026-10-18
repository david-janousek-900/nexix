# Historical archives (task 87)

`make extract-v7-archives` extracts all six libplot source archives and all six
learn lesson archives to `build/v7-archives`. Choose a fresh directory with
`ARCHIVE_OUTPUT=build/another-directory` to repeat extraction. Existing output
is refused. No archived program is executed and reference bytes stay unchanged.

The reader implements V7's 0177545 magic and 26-byte headers explicitly: 14-byte
names, PDP-11 word-ordered 32-bit dates and sizes, byte UID/GID, little-endian
16-bit mode, and even-byte payload alignment. It rejects truncated headers,
payloads and padding, duplicate names, path traversal and non-ASCII/control
names before writing a member. Host `ar` formats are not accepted.

Each extracted archive has its own directory. `provenance.json` records the
source path and SHA-256, every member's SHA-256, size and historical metadata.
Extraction keeps ordinary host data-file permissions rather than applying
historical ownership, executable bits or timestamps. This prepares source and
lesson inputs; compiling libplot and installing learn remain their own tasks.

`make test-v7-archives` checks high-word-first longs (including sizes above
64 KiB), odd alignment, malformed and unsafe inputs, no-overwrite behavior,
and byte-for-byte extraction/hash verification of every member of all twelve
pinned archives. It is included in `make test-unit`.

# File descriptors, file operations and pipes (step 13)

Nexix 0.1.0 now has native file syscalls, libc wrappers and process ownership
for files, directory references and pipes. The implementation adapts V7's
fio.c, sys2.c, sys3.c and pipe.c behavior to the native syscall and VM interfaces.
Original sources remain unchanged under `reference/`.

## Test and boot

```sh
make TOOLCHAIN_PREFIX= test-files
make TOOLCHAIN_PREFIX= smoke
make -j3 TOOLCHAIN_PREFIX= test
make TOOLCHAIN_PREFIX= run
```

`test-files` runs with 16 MiB RAM and saves `build/files-16M.log`; smoke uses
128 MiB. Successful boots require `NEXIX_FILES_OK`, `NEXIX_PIPE_OK`,
`NEXIX_FILES_CLEANUP_OK`, and the existing boot markers. The tests execute
`user/files-test.c` as a native ELF64 program at CPL3, linked with Nexix libc.
Boot still ends in the kernel idle loop. Step 14 adds interactive serial TTY
input and a demo; see [tty.md](tty.md). The V7 shell remains pending.

## Descriptors and ownership

Each process has 20 descriptor slots. The kernel has 175 open-file descriptions,
200 cached inode references and 32 pipe buffers. Open and dup select the lowest
free descriptor. An open-file description owns its offset and access/append
flags; dup and fork share that description. Separate opens have separate offsets.
Descriptor close-on-exec flags are independent, copied on fork and cleared by
dup/dup2. Dup2 closes an occupied target; duplicating a descriptor onto itself
preserves its flags.

Fork acquires file and cwd references only after cloning memory succeeds.
Successful exec closes descriptors marked close-on-exec, preserving other files
and cwd; failed exec preserves every descriptor. Exit closes all descriptors and
releases cwd references. Unsupported foreign inode/file or shared-text references
still return EBUSY at fork/exec.

`files_mount()` attaches one already mounted filesystem. Detaching or switching
returns EBUSY while managed references exist. These are internal kernel APIs,
serialized with IRQs disabled. The boot test attaches a disposable RAM fixture,
runs file tests, then detaches it. Permanent root attachment remains step 18.

## File and namespace calls

Libc exposes open, creat, close, read, write, lseek, dup, dup2, pipe, fcntl,
stat, fstat, chdir, link and unlink. Constants live in `include/nexix/file.h`
and are exported by `<fcntl.h>` and `<unistd.h>`. Their values are Nexix ABI
values; programs must use Nexix headers when rebuilding.

Open supports O_RDONLY/O_WRONLY/O_RDWR, O_CREAT, O_EXCL, O_TRUNC, O_APPEND and
O_CLOEXEC. Truncation requires write access and resets a regular file to zero,
reclaiming direct and indirect block trees. Creation applies the process umask
and effective UID/GID. Append selects current EOF for each write. Seek supports
SEEK_SET/CUR/END, rejects negative results/overflow, and permits seeking beyond
EOF; sparse writes use the existing filesystem allocation code.

Lookup checks directory search permissions at each component. File access uses
effective owner, group or other mode bits; UID 0 bypasses these checks. Namespace
changes require parent write and search permission. Relative paths use cwd;
absolute paths start at inode 2. V7's 14-byte component truncation is retained.
Link/unlink operate on regular files. Unlink removes a name immediately, but an
open inode with zero links stays usable until its last open-file reference closes.
Its blocks and inode slot are then reclaimed. Empty directory entry slots and
free inode slots are reused; directories grow when necessary.

Directories can be opened for reading their raw V7 directory records. Directory
writes and directory links/unlinks are rejected. Step 14 connects supported
character devices to the serial terminal; see [tty.md](tty.md). Other devices
return ENODEV.
There is no mkdir, rmdir, rename, symlink, mount syscall, chroot or supplementary
group handling in this milestone.

Fcntl supports F_GETFD/F_SETFD for FD_CLOEXEC and F_GETFL/F_SETFL for shared
append flags. F_SETFL preserves the original access mode. Stat/fstat export a
48-byte native record: eight 16-bit fields (dev, ino, mode, nlink, uid, gid,
rdev, reserved), followed by signed 64-bit size, atime, mtime and ctime.
Timestamps are decoded from disk but are not updated yet; root device ID is zero.
This record is separate from the historical PDP-11 disk layout.

User buffers are fully checked before I/O can move an offset, consume pipe data
or sleep. Invalid ranges return EFAULT without those side effects. Pipe validates
its eight-byte descriptor output before allocating resources. Later storage errors
can return a partial byte count. Filesystem write failures retain the existing
mount poisoning policy; metadata changes are synchronous but not crash atomic.
There is no journal or orphan recovery after reboot.

## Pipes

Each pipe uses a 4096-byte ring. Reads sleep while empty with writers present,
return available data, and return EOF once all writers close and buffered data
is consumed. Writes sleep when full; writes of at most PIPE_BUF (4096 bytes)
wait for enough space for the whole write and cannot interleave with another
writer. Larger writes can block between chunks. These rules use V7 sleep/wakeup
under the single-CPU scheduler.

Pipe ends survive dup/fork and remain open until the final reference to their
open-file description closes. Closing a final end wakes blocked peers. Writing
with no readers returns EPIPE (or a partial count after earlier progress).
Step 15 adds SIGPIPE and interruptible I/O; see [signals.md](signals.md). There is no
nonblocking mode, select or poll. Lseek on a pipe returns ESPIPE; fstat reports
FIFO mode and the current buffered byte count.

## Validation coverage

The C fixture checks regular I/O, descriptor errors and exhaustion, shared
offsets across dup/fork, dup2 replacement, umask, append, stat, sparse-compatible
file growth, create/exclusive-open/truncate, links, open-unlinked files, cwd,
failed/successful exec and close-on-exec. A separate nonroot run checks permissions.
Pipe tests cover invalid buffers without data consumption, endpoint errors,
transfers beyond buffer capacity, EOF on child exit and EPIPE. A private active
`int 0x81` test hook waits until a writer is demonstrably asleep on a full pipe,
then the reader closes to verify wakeup. All actual file operations use int 0x80.

Three repeated full runs check pool reuse and exact page/block/inode counts after
exit. Host tests additionally reclaim single/double/triple trees, exhaust/reuse
inode slots, grow directories across sectors, remount namespace changes, exercise
read-only mounts and verify owner/group/other search permissions. These complement
the existing malformed metadata and injected device-I/O failure tests.

Step 14 implements serial TTY and terminal handling; see [tty.md](tty.md).

# Root and PID 1 startup (task 18)

Fresh full IDE installations now boot to `login:` on both serial lines. See
[multi-user startup and credentials](multi-user.md). The single-user startup
described below remains the fixed RAM recovery path and its `test-boot` fixture.

Run `make build` to create boot artifacts without launching QEMU, or `make run`
to build and boot, on an x86_64 Linux host with GCC/binutils. Host tools are the
default; an installed cross-toolchain can be selected with
`make TOOLCHAIN_PREFIX=x86_64-elf- run`.
GRUB loads Nexix, the kernel initializes memory, interrupts and the serial TTY,
then mounts the primary IDE master as the root. `make run` attaches
`build/nexix.img`, initially copied from the packaged `build/root.img`.
Without that device, the kernel uses a writable RAM copy of the packaged root.
It reads `/etc/init` through the V7 filesystem and builds a native ELF64 user
address space. The first process receives PID 1; init forks the initial shell,
which receives PID 2 and opens `/dev/console`. The console displays `# `.

Try:

```sh
pwd
ls /bin
echo hello > /tmp/message
cat /tmp/message
cp /bin/echo /tmp/my-echo
/tmp/my-echo hello
rm /tmp/message /tmp/my-echo
```

Ctrl-D on an empty line closes the single-user shell. Init waits for it,
runs `/etc/rc`, reads the empty `/etc/ttys` configuration and starts another
single-user shell. The historical V7 shell's interactive `exit` returns to
its command prompt; use Ctrl-D for this lifecycle. Init remains PID 1 with process group zero while replacement shells get new
PIDs and join the terminal group on console open. Init termination is a fatal kernel error.
Ctrl-A, release, then X exits QEMU. Host signal processing is disabled on
QEMU's multiplexed console so terminal control bytes can reach Nexix.

IDE root changes survive QEMU restarts. See [storage.md](storage.md) for disk
creation, preservation and resetting. Diskless boots use a fresh RAM root.
Getty/login and multi-user startup are not implemented.
The mounted root has the current working directory fallback `/`; a chdir
acquires a managed inode reference normally.

Normal exec resolves absolute or cwd-relative paths through this root, checks
search/execute permissions using effective credentials and loads regular ELF
files directly from their inode. The loader reads headers and segments through
a bounded callback, populating allocated pages without copying the whole file
into a kernel buffer. Short reads become EIO. Failed loads free staged pages
and leave the caller's address space and descriptors intact. Set-ID executable
modes are rejected with ENOEXEC; privilege-changing exec is not implemented.
There is no fallback to embedded executable names during normal operation.

`make TOOLCHAIN_PREFIX= test-boot` tests normal startup at 16 MiB and 128 MiB RAM.
It checks PID 2, the root directory, the packaged utilities, writable file I/O,
a newly copied executable and shell restart through actual COM1 input.
Transcripts are `build/boot-16M.log` and `build/boot-128M.log`.
Markers `NEXIX_ROOT_MOUNTED`, `NEXIX_INIT_PID1` and `NEXIX_BOOT_OK` identify
kernel startup; the received prompt and command results verify init/shell execution.

The regression ISO uses `test=selftest` to run the earlier fixtures and idle
instead of starting init. `make TOOLCHAIN_PREFIX= smoke` and component test
targets build `build/test-selftest.iso` automatically. Fault, memory and TTY
modes retain their dedicated ISOs. Regression-only mounts use the embedded exec
catalog for historical fixtures; `files_mount_root` enables filesystem exec.
The complete suite also checks missing/unexecutable/malformed images, relative
paths, V7 set-ID exec and credential rollback, copied executables and resource reclamation.
ELF callback error tests inject failures in header, program-header and segment
reads, checking rollback after each failure.

Task 19 now verifies interactive editing, interrupts, scripts and longer
sessions; see [interactive-shell.md](interactive-shell.md).

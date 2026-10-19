# Serial TTY and terminal handling (task 14)

Nexix 0.1.0 now connects COM1 to native character-device file operations and a
V7-style line discipline. The retained `reference/v7/usr/sys/dev/tty.c` and
`reference/v7/usr/sys/h/tty.h` provide the behavior and sgtty/ioctl definitions.
The x86_64 implementation uses fixed queues and the existing scheduler rather
than PDP-11 clists and device drivers.

## Try the terminal

```sh
make TOOLCHAIN_PREFIX= run-tty
```

Wait for `serial TTY demo; type a line, or quit to finish.` Type a line
and press Enter; the program prints `received: ` followed by that line.
Backspace edits the current line, Ctrl-U discards it, and Ctrl-D on an empty
line ends input. Type `quit` and Enter to finish and reach the kernel idle loop.
Press Ctrl-A, release it, then X to exit QEMU. The interactive target uses QEMU's
stdio multiplexer with host signal processing disabled so control bytes reach
the guest. Ctrl-A followed by A sends a literal Ctrl-A to the guest.

The demo is a native C test program, not the V7 shell. Normal `make run` runs
self-tests and idles; permanent root mounting and init/shell startup remain
tasks 17–19. Both modes use the same terminal and file syscall implementations.

Automated checks:

```sh
make TOOLCHAIN_PREFIX= test-unit
make TOOLCHAIN_PREFIX= test-tty
make TOOLCHAIN_PREFIX= smoke
make -j3 TOOLCHAIN_PREFIX= test
```

`test-tty` checks real COM1 input with 16 MiB and 128 MiB RAM, then exercises the
demo with 16 MiB. Logs are `build/tty-16M.log`, `build/tty-128M.log` and
`build/tty-demo-16M.log`. Normal successful boots require `NEXIX_TTY_USER_WRITE_OK`,
`TTY OK` and `TTY CLEANUP OK`; UART tests also require
`TTY UART OK`, and the demo requires `TTY DEMO OK`.

## UART and character devices

COM1 uses port 0x3f8, a 16550-compatible FIFO and PIC IRQ4 (vector 36). Receive
and line-status interrupts are enabled; transmit interrupts remain disabled.
The IRQ handler drains a bounded number of bytes, rejects parity/framing/break
errors and feeds the line discipline. Receive interrupts can wake sleeping
processes. Timer initialization preserves other enabled PIC IRQs.

Writes poll the transmitter. Baud/format changes wait for the hardware
transmitter to empty and update the divisor and line-control registers with IRQs
disabled. Supported speed codes are B50 through B9600, EXTA (19200), EXTB (38400)
and the Nexix extension B115200. Input and output speeds must match because they
share one UART divisor. B0/hangup and invalid codes return EINVAL.

The RAM filesystem fixture contains:

| Path | Device | Purpose |
|---|---|---|
| `/dev/console` | 0x0100 | COM1 terminal |
| `/dev/ttyS0` | 0x0100 | Second name for the same inode/terminal |
| `/dev/tty` | 0x0200 | Calling process's controlling terminal |

Device inode permissions are checked by open. The fixture uses root-owned mode
0600. Character-device output works even on a read-only filesystem mount, since
it writes the device rather than filesystem blocks. Unknown device numbers
return ENODEV. Opening `/dev/tty` before acquiring a controlling terminal returns
ENXIO. Block-device file dispatch is still pending.

The first console open associates the process with the static COM1 terminal;
fork and exec preserve that association. Open-file descriptions hold terminal
opens, while dup/fork share their description. The last close flushes input and
clears software output stop; the next first open restores default settings.
Exit clears the process's controlling-terminal association after closing its
files. Terminal descriptors cannot seek (ESPIPE); stat/fstat report the character
inode and encoded device number.

## Line discipline

The default is cooked mode with ECHO and CRMOD, 115200 baud and 8N1. Default
controls are DEL (also accepting Backspace) for erase, Ctrl-U for kill, Ctrl-D
for EOF, Ctrl-S for output stop and Ctrl-Q for output start. These editing
controls suit current serial terminals; historical V7 defaults `#`/`@` can be
selected through sgtty. Interrupt and quit characters are stored as Ctrl-C and
Ctrl-backslash. Task 15 posts SIGINT/SIGQUIT to the terminal group, flushes
input and resumes output; RAW keeps these bytes as ordinary data.

Cooked input accumulates an editable line until newline, a configured break
character, or EOF. CRMOD converts incoming CR to LF and outgoing LF to CR/LF.
Reads return at most one complete record; short reads leave the remainder for
the next read. EOF after text completes that record without adding a byte. EOF
on an empty line produces one zero-byte read and leaves later input usable.
Reads with no completed record sleep on the input channel.

CBREAK makes received bytes immediately readable while retaining echo, CR
mapping and software flow control. RAW preserves all eight bits and treats
control bytes as ordinary data; it bypasses CR/tab processing and software flow
control. Nonraw input/output use seven-bit characters. ODDP or EVENP selects
7O1 or 7E1; specifying both uses 8N1, as does RAW. XTABS expands outgoing tabs to
eight-column stops. Default erase echo uses Backspace/space/Backspace, and kill
starts a new line. This is basic byte editing, without quoting or screen editing.

Ctrl-S stops user writes; a writer sleeps until Ctrl-Q, a flushing parameter
change, raw mode or a control-character change releases it. Receive handling
never sleeps. Echo is suppressed while output is stopped. Kernel boot/panic
messages use direct serial output and bypass terminal flow control.

The unfinished line holds at most 256 bytes, reserving space for its delimiter.
Further text is discarded until the line can be completed. The completed input
ring holds 1024 entries; a cooked record that does not fit is discarded as a
whole, and raw/cbreak overflow drops new bytes. Bytes received with no terminal
opens are ignored. No heap or interrupt-time allocation is needed.

## sgtty and ioctl ABI

`<sgtty.h>`, `<sys/ioctl.h>` and `<unistd.h>` expose stty, gtty, ioctl and isatty.
The syscall numbers are the V7 numbers: stty=31, gtty=32 and ioctl=54. Ioctl
passes fd, request and a native pointer; its libc wrapper takes three arguments,
using NULL for requests without a buffer. Requests retain V7's numeric values.

`struct sgttyb` is eight bytes: four byte-sized fields (input speed, output speed,
erase, kill), followed by 32-bit flags. `struct tchars` is six bytes (interrupt,
quit, start, stop, EOF, break). TIOCGETD/TIOCSETD use a native 32-bit integer;
only line discipline zero is implemented. Disk records are unaffected.

| Request | Behavior |
|---|---|
| TIOCGETP | Get sgtty parameters |
| TIOCSETP / stty | Validate, flush input, set parameters and release output stop |
| TIOCSETN | Set parameters, retaining input when raw/cbreak mode does not change |
| TIOCGETC / TIOCSETC | Get/set control characters; setting releases output stop |
| TIOCGETD / TIOCSETD | Get/set discipline zero; others return ENXIO |
| TIOCFLUSH | Flush input and release software output stop |
| FIOCLEX / FIONCLEX | Set/clear descriptor close-on-exec, including regular files and pipes |

Changing raw/cbreak mode flushes queued input even with TIOCSETN, preventing old
record markers from being interpreted under a different mode. Parameters and
user pointers are validated before changing settings or flushing input. Get/set
buffer failures return EFAULT, unsupported flags/rates return EINVAL, and unknown
TTY requests or terminal requests on regular files/pipes return ENOTTY. Bad
file descriptors return EBADF. Supported flags are RAW, CBREAK, ECHO, CRMOD,
ODDP, EVENP and XTABS; delay modes, LCASE and TANDEM are not implemented.

There is no asynchronous output queue, nonblocking I/O, termios, window-size
handling, modem/carrier detection, hangup policy or terminal job control yet.
Task 15 implements signal delivery and interruptible terminal I/O; see
[signals.md](signals.md).

## Validation

Host tests exercise the production line discipline with captured output and
user-copy stubs: editing and echo, records/EOF/partial reads, custom controls,
queue wrap and overflow, raw/cbreak modes, CR/tab expansion, baud/parity selection,
parameter validation before flushing, and last-close/default reuse.

The native C fixture tests device descriptors, ioctl and legacy stty/gtty,
permissions through the existing file layer, read-only mount output, isatty,
invalid user buffers without input loss, controlling-terminal alias resolution,
blocking read wakeup, descriptor sharing and fork/exec/exit cleanup. A private
active test gate injects input for normal boot tests and waits for a child to be
demonstrably asleep before waking it. All file/control operations use int 0x80.

The dedicated UART run disables test-gate input injection and exchanges bytes
through QEMU's actual COM1 chardev: cooked editing, raw NUL/high-bit bytes, a
cbreak byte without newline, Ctrl-C/Ctrl-backslash group signals interrupting
a sleeping child, and Ctrl-S/Ctrl-Q with a demonstrably sleeping
writer. The automated demo waits for the response before sending the next line,
so concurrent input echo does not obscure its output check. After each test,
all process pages, descriptor/inode references and terminal opens are reclaimed.

Task 68 adds [V7 serial packets](serial-packets.md), TIOCSETD/DIOCGETP
negotiation and libc pkon/pkoff/turnon. Run `make test-packet` for host
protocol/ownership/UART/libc checks and real QEMU COM1 exchanges at 16/128 MiB.

Task 69 implements [native memory/special devices](memory-devices.md), including
privileged mem/kmem byte access, null aliases and controlling-TTY acceptance.
Run `make test-memdev` for host and 16/128 MiB syscall tests.

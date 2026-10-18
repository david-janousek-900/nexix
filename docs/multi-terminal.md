# Independent terminal implementation

Task 89 replaces the COM1 singleton state with two bounded terminal objects.
Each owns its queues, editing state, process group, carrier/settings, output delay,
flow control, exclusive-open flag and mpx ownership. Driver callbacks bind output,
formatting and modem operations to the appropriate transport. Unbound devices
return ENODEV; attaching over open descriptions returns EBUSY.

The primary device family uses major 1, minor 0/1. Console and historical profile
aliases continue using terminal zero; /dev/tty follows a valid controlling-terminal
pointer. Per-line receive/error and modem entry points prevent cross-terminal
routing. Existing COM1 entry points remain compatible with private fixtures.

Host acceptance uses two independently captured transports, distinct users/groups,
raw and canonical queues, settings, output, exclusive opens and close lifetime.
COM1 controller, modem, mpx, packet and real UART regressions remain required.
Tasks 91–93 cover controlling terminals, packaging and concurrent acceptance.

Task 90 binds a detected PC 16550 COM2 at 0x2f8 to terminal one, with IRQ3
(remapped vector 35), initially at 115200 baud, 8N1. A scratch-register probe
leaves absent hardware unbound and IRQ3 masked. Its independent 4096-byte TX ring
rejects oversized enqueues atomically; bounded IRQ work drains TX and routes RX,
line errors and modem changes only to line one. Clear-on-read receive error bits
survive transmit/idle polling. COM1 remains the boot and panic console.

This hardware adapter is native x86 code; V7's terminal discipline and controller
contracts are represented by `reference/v7/usr/sys/dev/tty.c`, `kl.c` and `dh.c`.
It does not pretend that the PDP-11 register drivers operate PC UARTs directly.

`make test-serial-secondary` checks missing/present UARTs, IRQ masking, ring wrap,
backpressure, RX errors, break and modem signals with a register model. It also
boots disposable IDE roots at 16/128 MiB with two QEMU serial pipes, receives all
byte values on COM2, transmits 8192 binary bytes, verifies ordered output and
checks that COM1 remains usable. The private test creates its own device node;
normal `/dev/ttyS1` packaging follows in task 92. These tests preserve the working
disk and the reference snapshot.

Task 91 validates the per-line controlling-terminal and process-group behavior
already supplied by task 89 and the existing V7 process/file lifecycle. The user
area's terminal pointer and device number survive fork and exec; exit drops file
references and clears that process's association. The last line close drains or
flushes output, clears its queues and releases its terminal group. Opening another
physical line does not replace an existing controlling-terminal association.

The V7 source contracts are `usr/sys/dev/tty.c` (ttyopen, signals and close),
`usr/sys/sys/slp.c` (newproc), `sys1.c` (exec/exit), and `sys2.c` (file close).
Paths are relative to `reference/v7/`. Nexix retains its existing native
`setpgrp()` convenience call; the original V7 sysent slot is unused. A child of
init starts without a controlling terminal, forms its group and opens its line.
No POSIX session API is introduced.

`make test-terminal-sessions` boots a private init fixture on a disposable IDE
volume at 16/128 MiB. Two children acquire distinct UARTs and groups, exec, fork
again, and exec again; writes through `/dev/tty` must reach the correct endpoint.
The test checks cooked erase, empty newline, EOF, interruptible reads and Ctrl-C
group isolation. Stopping COM1 with Ctrl-S must leave COM2 running and finishing;
Ctrl-Q resumes COM1 and both sessions close/reap successfully. This fixture tests
the native lifecycle without depending on the forthcoming getty/login ports.
Production init's startup policy and respawn acceptance remain task 101.

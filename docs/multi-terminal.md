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
Full controlling-terminal acceptance follows in 91–93.

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

# Task 74: terminal-controller contracts

The source responsibilities in `reference/v7/usr/sys/dev/{tty,dc,dh,dz,kl,partab}.c`
are mapped to the native TTY, controller helpers and interrupt-driven 16550 UART.
This is a source-based adaptation: PDP-11 registers, DMA and receive silos are
replaced by the native UART queues and timer. No reference files are modified.

## Endpoints and parameters

`/dev/dc0`, `/dev/dh0`, `/dev/dz0` and `/dev/kl0` select profiles on the **same
COM1 endpoint**. Close all current terminal descriptions before changing profile;
a different profile returns EBUSY while the endpoint is open. `/dev/console` and
`/dev/tty` follow the active profile; opening console with no existing opens
restores the native profile. These nodes do not provide four independent lines.
The reference line bounds (DC 4, DH 16, DZ 16, KL 1) are checked: other valid
lines return ENODEV and out-of-range lines return ENXIO. Independent terminal
queues, COM2 and multi-terminal operation remain tasks 89–93.

| Profile | Initial parameters | Controller behavior |
| --- | --- | --- |
| Native | 115200, ECHO, CRMOD | Existing native erase DEL, kill Ctrl-U, interrupt Ctrl-C |
| DC | 150, ANYP, ECHO | Source speed selectors 134/150/300/1200; software even output parity |
| DH | 300, ANYP, ECHO | Cooked 7-bit parity, raw 8-bit; 134 uses 6-bit parity; 110 output uses two stop bits |
| DZ | 300, ANYP, ECHO | Source nonzero speed selectors; cooked 7-bit parity, raw 8-bit; 110 input uses two stop bits |
| KL | Fixed physical 115200, EVENP, LCASE, ECHO, XTABS, CRMOD, CR2 | Software even output parity; speed fields do not change hardware |

Historical profiles use erase `#`, kill `@` and interrupt DEL. DH/DZ select even
parity when EVENP is present, otherwise odd. DC/KL add even parity to cooked
output regardless of parity flags; raw output preserves all eight bits. DC input
checks software parity against ODDP/EVENP, including raw mode. DH/DZ reject parity
errors when exactly one parity flag is selected; frame/break produces raw NUL
or cooked DEL. DC discards frame/break errors; KL follows the source by passing
received bytes without interpreting UART error flags. Native error policy is
unchanged. Overrun does not discard the available byte.

The 16550 shares one receive/transmit clock, so unequal rates are rejected for
non-KL profiles. B0 hangup and unsupported selectors return EINVAL atomically.
DH external selectors use native 19200/38400 rates; PDP-11 external clock and
134-baud half-duplex wiring are not emulated. Modem/carrier behavior, including
B0 carrier transitions, remains task 75; the current endpoint is locally attached.

## Shared line discipline

TANDEM sends XOFF at 128 buffered input bytes and XON below 51. Native input
capacity remains 1024 bytes. One XOFF is sent per high-water transition rather
than repeatedly enqueueing it as the source does. Changing start/stop characters
releases an outstanding stop with the original start byte; deferred releases
retry as output space becomes available. Existing native explicit-XON resume
behavior is retained.

LCASE implements source input case folding (including raw mode), canonical
backslash mappings and escaped erase/kill characters. Cooked output implements
uppercase conversion and escaped punctuation, CRMOD, XTABS and EOT suppression.
The source `partab` character classes and parity bits are preserved.

Output uses 2048 typed byte/delay tokens before the UART software queue. Delay
starts after preceding UART output has physically drained. Source 60 Hz delays
are rounded up to native 100 Hz ticks, with source DC/KL four-tick and DH/DZ
six-tick padding. NL1/NL2, TAB1, CR1/CR2 and VT delays follow the source formulas;
other encoded delay selections and BSDELAY retain the source's no-delay behavior.
Raw output bypasses transformations, delays and column tracking. Writes apply
backpressure and remain interruptible; packet discipline retains its binary path
and cannot attach over pending line-discipline output.

TIOCSETP drains output and flushes input; TIOCSETN retains input and waits for
wire-idle before changing hardware format. TIOCFLUSH cancels queued output and
delays. TIOCEXCL/TIOCNXCL reject subsequent non-root opens while exclusive;
TIOCHPCL drops DTR on final close. Final close drains output, or cancels stopped
output, and releases input flow control. DTR handling does not imply carrier or
modem signal emulation.

## Validation

`make test-tty-controllers` runs controller and UART register-model tests,
reference-table comparisons, root filesystem and inventory/libc audits, QEMU
TTY tests and binary packet peer tests. Root acceptance exercises profile rebinding,
error injection, canonical input, TANDEM and real timer delays at 16 and 128 MiB.
The private receive-error injection gate is unavailable in normal boots.

Host tests cover clear-on-read UART status preservation, parity and break policies,
source speed limits, output backpressure, wire-idle delay barriers, flush/drain,
exclusive opens and DTR. `tools/check_tty_controller.py` compares the original
partab/map tables and DC/DZ speed selectors against native helpers. Shell and
persistent-storage regressions also run before commit; the default build is
checked to preserve an existing persistent disk image.

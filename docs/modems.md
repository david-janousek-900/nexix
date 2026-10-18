# Task 75: modem and dial-control equivalents

The V7 sources are `usr/sys/dev/dn.c` (DN-11 dialer), `dhdm.c` (DM-BB
carrier control), `dhfdm.c` (alternative always-carrier implementation) and
`du.c` (DU-11 synchronous interface), with `dh.c` and `tty.c` integration.
The native implementation is `kernel/v7/modem.c`, the TTY carrier adapter and
16550 modem-status interrupts. The reference tree remains unchanged.

## Terminal carrier

`NX_MDM_GET` returns a 16-byte `nx_modem_tty` record containing mode, input
signals and output leads. Root can select a mode with `NX_MDM_SET`; only its
`mode` field may be nonzero. The declarations are in `<nexix/modem.h>`, also
included by `<sys/ioctl.h>`. Existing sgtty records and syscall numbers are
unchanged.

| Mode | Carrier source |
| --- | --- |
| LOCAL (default) | Always present, corresponding to `dhfdm.c`; console boot remains unchanged |
| UART | COM1 MSR DCD, CTS, DSR and RI inputs, updated by modem-status interrupts |
| VIRTUAL | Privileged native peer supplies input transitions |

Changing mode establishes the new baseline without synthesizing SIGHUP. Opens
in carrier-controlled modes assert DTR/RTS and sleep interruptibly for DCD.
A failed/interrupted open releases its reservation. The source DH secondary
receive indication selects 1200 baud on open; virtual peers can supply it,
while the 16550 has no corresponding secondary receive pin.

Carrier loss on an established terminal sends SIGHUP to its process group
(or mpx port), closes packet discipline, flushes input/output, drops DTR/RTS
and wakes blocked I/O. Waiting opens receive a wakeup without a hangup signal.
Reads without carrier return EOF; writes return EIO or an already-written
prefix. Repeated low-carrier samples do not repeat SIGHUP. UART mode uses real
16550 MSR interrupt acknowledgement; virtual transitions use the same TTY path.

DH/DZ profiles accept both speed fields set to B0, cancel queued data and drop
modem leads. A subsequent supported nonzero setting restores leads and carrier
according to the selected mode. B0 does not program a zero UART divisor.
TIOCHPCL drops both leads on final close. LOCAL mode retains the native console
policy; independent terminal hardware and queues remain tasks 89–93.

## Dial and synchronous endpoints

The installed nodes are root-only character devices:

| Node | Contract |
| --- | --- |
| `/dev/dn0` through `/dev/dn3` | Four independently reserved DN lines; writes dial bytes and await connection |
| `/dev/du0` | One DU synchronous endpoint; process-owned opens and shared inherited descriptions |
| `/dev/modemctl` | Privileged peer/control endpoint |

These DN/DU endpoints are production virtual backends, not UART byte-stream
aliases. A 16550 does not implement DN automatic-call hardware or DU synchronous
clocking. A privileged process can bridge the peer API to an external service
or drive deterministic protocol exchanges. No PDP-11 register addresses, DMA
or electrical synchronous clocking are emulated. Closing the controller does
not unplug the peer; signal state persists until explicitly changed.

`NX_MDM_PEER` exchanges a fixed 536-byte `nx_modem_peer` record: device,
operation, signals, count, returned state, zero reserved field and 512 payload
bytes. Root privilege is checked for every ioctl, including inherited
controller descriptors. The complete output buffer is validated before TAKE
can consume any data. Unsupported minors, operations, signal bits, reserved
fields and oversized payloads are rejected. DN/DU reads and writes use the
ordinary file layer's permission checks, user-buffer preflight and timestamp
updates. Dup/fork retain the endpoint until the final file description closes;
mpx rejects these endpoints as non-TTY devices.

Peer operations are STATUS, SIGNALS, TAKE, RECEIVE, END and TTY. TTY targets
`/dev/console` in VIRTUAL mode; it accepts DCD/CTS/DSR/RI/SECONDARY bits.
SIGNALS sets DN status or DU modem inputs. State reports OPEN and
READ/WRITE/PENDING as appropriate; DU state also reports asserted DTR/RTS
using the corresponding lead bits. Controllers use STATUS and TAKE to perform
handshakes rather than assuming a write has completed.

### DN

Open rejects an occupied, powered-off or already-reserved line. READY permits
one digit; TAKE collects it, then the peer supplies READY for the next digit.
A `-` consumes a two-second pause without transmitting a digit. The writer
waits after its final digit until CONNECTED, or returns EIO on POWER/ABANDON.
Signals interrupt waits with EINTR. As in V7 `dnwrite`, connection failure is
a call-level error even if digits have already been consumed. Concurrent
writers serialize the dialing sequence. Final close releases the reservation
and discards pending digits. Read is unsupported. The native pause uses a full
two seconds, replacing the original two once-per-second lbolt wakeups.

### DU

The first open allocates a fixed 512-byte buffer; additional opens are accepted
only from the owning process, while inherited descriptors remain usable. DSR
is required for I/O. RECEIVE appends a bounded record, preserving the source's
high-bit receive-error representation. END, carrier loss or receive timeout
finishes reception. Reads truncate to the caller's size and, like V7 `duread`,
do not consume the buffered record.

Writes wait for receive completion, copy at most 512 bytes, assert the native
pending RTS phase and wait interruptibly for CTS. Once transmitting, TAKE
collects bytes in order and refreshes the ten-second activity timeout. The
last byte turns the endpoint back to receive mode. Timeout returns pending
transmission to receive mode; idle reads return EOF. DSR loss returns EIO.
An interrupted or timed-out CTS handshake cancels pending transmission. This
bounds the original driver's possible indefinite CTS wait after timeout.

Receive and transmit directions share one source-style buffer; native I/O
serialization prevents overlapping callers from corrupting it. Final close
clears ownership, buffered data and timers. Electrical sync stripping and
parity are represented by peer records rather than a physical DU register.

## Validation

`make test-modem` includes host protocol tests, a 16550 register model, root
acceptance at 16/128 MiB, inventory/ABI/libc audits, real UART TTY and packet
regressions. Host tests cover all four dial units, exclusivity, pause timing,
connection/error/interrupt outcomes, DU owner checks, repeated/truncated reads,
capacity, CTS/DSR transitions, timeout and close/reopen, destructive-read
preflight, privileged peer access, carrier waits, repeated hangups and B0.
Mpx tests reject ioctl payloads larger than its existing eight-byte ABI.

QEMU acceptance uses ordinary device opens/ioctls, forked peers, dup and last
close, real timer-driven dialing pauses, synchronous binary records, privilege
drop and SIGHUP delivery to an isolated terminal process group. It prints
`NEXIX_MODEM_OK`; no private modem injection syscall is needed. The default
persistent disk is preserved, and shell/storage regressions run before commit.

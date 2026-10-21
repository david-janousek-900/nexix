# e1000 DMA driver (task 417)

`kernel/net/e1000.c` drives QEMU's Intel 82540EM with legacy descriptors. It
builds on [PCI discovery](networking-pci.md), the
[packet pools](networking-pools.md) and the
[network worker](networking-worker.md). It is a fresh native implementation
under Nexix's root BSD-3-Clause license. Task 418 attaches it as the
[`em0` interface](networking-interfaces.md); nothing starts the stack during
normal boot yet.

## Start and stop

`e1000_start(pool, input, context)` runs in process context while the worker
is running.

- **Reset.** It masks every interrupt cause, disables RX and TX, sets
  `CTRL.RST` and polls until the device clears it, giving up after a bounded
  wait. It then reads ICR to discard stale causes.
- **MAC address.** The MAC comes from EEPROM words 0–2 through `EERD` and is
  programmed into receive address 0 with the address-valid bit set. The
  multicast table is cleared, and link-up (`SLU`/`ASDE`) is set.
- **Memory.** Two descriptor-ring pages and 32 buffer pages come from the page
  allocator, all of them identity-mapped below 1 GiB, so a virtual address is
  also the bus address. If any allocation fails, everything already acquired is
  released.
- **Rings and receive setup.** Each ring has 32 descriptors of 2 KiB each, so 31
  frames can be in flight. RX gives the hardware every slot except one
  (`RDT = 31`). `RCTL` accepts broadcasts, strips the FCS and uses 2048-byte
  buffers.
- **Transmit setup.** `TCTL` pads short frames and uses the documented
  collision settings with standard IPG timing. TX descriptors start out marked
  completed.
- **Interrupts.** The firmware-routed INTx IRQ is attached last. The enabled
  causes are TX written-back, link change, RX minimum threshold, RX overrun and
  RX timer.

`e1000_stop`, called from the worker, makes it safe to free DMA memory. It
masks the device's interrupts, detaches the IRQ, then resets the device
(clearing `RCTL`/`TCTL`) so that no DMA is still in progress. Only then does it
cancel pending work and free every ring and buffer page. The adapter can be
started again afterwards.

## Interrupts and the worker

A generic `irq_attach` registers handlers for PIC lines 5–13, with the cascade
unmasked for slave lines. `irq_detach` masks the line again. The e1000 handler
runs in IRQ context before EOI and never sleeps:

- It reads `ICR`, which acknowledges the level-triggered INTx. Zero means a
  shared line that isn't ours.
- It ORs the cause into a pending word, then calls `net_worker_submit` for the
  driver's work item; repeated submits coalesce.

All descriptor and buffer access happens in the worker. `e1000_poll`:

- consumes the pending causes and updates link state on LSC;
- counts overruns and adds `MPC`;
- reclaims completed transmits;
- services up to one ring's worth of receive descriptors, re-queuing itself if
  more are complete.

## Transmit and receive

`e1000_transmit(frame)` runs in the worker and consumes its mbuf chain.

- Frames shorter than 14 bytes or longer than 1514 are rejected and counted.
- When the ring is full (`tail + 1 == clean`), the frame is dropped and
  counted.
- Otherwise the chain is copied into the slot's DMA buffer and the descriptor is
  written with EOP, IFCS and RS before `TDT` is advanced. The device appends the
  FCS and pads frames shorter than 60 bytes.

Receive copies each frame into a fresh packet-header mbuf chain from the pool
and passes it to `input`, which takes ownership in worker context.

- `e1000_rx_classify` rejects error bits, lengths outside 14–1514, and frames
  that span descriptors. A spanning frame is dropped whole and counted once.
- If the pool is exhausted the frame is dropped and counted.
- Every slot is cleared and handed back through `RDT`.

Counters cover frames and bytes in each direction, malformed and dropped
receives, missed receives, rejected and full transmits, completions, IRQs, link
changes and overruns.

## Validation

`make test-e1000` runs `e1000_rx_classify` under ASan/UBSan. It covers the
length boundaries, every error bit, and multi-descriptor frames, including a
final piece that looks valid.

`make test-e1000-guest` (the same boots as `test-pci-guest`) runs the driver
twice in a row against QEMU user networking at 16 MiB and 128 MiB, each time
starting it, testing and stopping it:

- start fails while the worker isn't running, and starting twice is refused;
- the MAC is QEMU's `52:54:00:...` and the link is up;
- ARP for the gateway 10.0.2.2 is answered;
- 100 ICMP echoes of 60–1514 bytes are reflected byte-for-byte with valid IPv4
  and ICMP checksums, wrapping both rings three times;
- with the worker busy, exactly 31 transmits are accepted and 5 are refused as
  ring-full, and all queued frames still go out;
- with receive left unserviced the ring fills to 31 frames and the overrun is
  reported, then reception resumes;
- malformed transmits (0, 13 and 1515 bytes) are rejected and their mbufs freed;
- synthesized RX descriptors (too long, too short, CRC error, a frame spanning
  two descriptors) are dropped and counted, with ring ownership kept consistent
  by pausing the receiver and moving `RDH`;
- traffic works again afterwards;
- stopping releases every page and mbuf, and the IRQ can be attached again.

With `-nic none` the boot prints `E1000 ABSENT` and completes.

**QEMU quirks.** QEMU holds receive traffic for about a second after `RCTL` is
written, so the test retries ARP. Its PHY loopback drops frames during that
window, so the tests use real echo traffic instead of loopback. Its `MPC` counts
each retry of a held frame, not frames lost, so ring-full tests check that the
condition was reported, not exact loss accounting. Real hardware drops frames
and counts them in `MPC`.

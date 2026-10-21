# Ethernet and loopback interfaces (task 418)

`kernel/include/net/if.h` and `kernel/net/if.c` adapt the interface layer of
4.4BSD `net/if.c`, `if_ethersubr.c` and `if_loop.c`. `kernel/net/stack.c` brings
up the live stack. These are fresh native implementations under Nexix's root
BSD-3-Clause license. Every interface function runs in
[network-worker](networking-worker.md) context and never sleeps. **Any function
that takes an mbuf chain consumes it**, whether it succeeds or fails.

## Interfaces

`struct nx_net_if` is the native counterpart of BSD's `ifnet`.

- **Fields.** It holds a NUL-terminated name (at most 7 characters), flags
  (`UP`, `RUNNING`, `BROADCAST`, `LOOPBACK`), MTU, link address, driver
  operations (`up`, `down`, `transmit`) and the BSD counters: input/output
  packets, bytes and errors, input queue drops and no-protocol drops.
- **Attach.** Up to 4 interfaces can be attached. Attach rejects duplicate,
  empty or already-attached interfaces and leaves the interface down.
- **Up and down.** `nx_net_if_up` calls the driver and sets `UP|RUNNING` only if
  that succeeds. `nx_net_if_down` clears both flags, calls the driver and frees
  any queued packets.
- **Detach** brings the interface down first.
- **Results.** Outcomes are a private `enum nx_net_result`. Task 424 maps it to
  socket errno values, so no ABI is added here.

Upper protocols register one input routine per ethertype (at most 4).
Duplicates are refused. A payload of an unregistered type is dropped and counted
in `noproto`, as BSD does.

## Ethernet

`nx_net_if_output(ifp, packet, dst, type)` handles output.

- It requires a packet header and a running interface.
- It refuses payloads larger than the MTU (1500).
- It prepends the 14-byte header (destination, own address, type) and passes the
  frame to the driver. A refusal from the driver counts as an output error.

`nx_net_ether_input` handles input.

- It drops input on a down interface.
- It counts as input errors frames shorter than 14 bytes, longer than 14 + MTU,
  or without a packet header.
- It pulls up a header that is split across buffers.
- It accepts only frames addressed to the interface's own address or to
  broadcast; there is no promiscuous or multicast receive yet.
- It strips the header and hands the payload, with an adjusted packet length,
  to the registered protocol.

## Loopback

`lo0` has the BSD loopback MTU of 16384 and a bounded input queue
(`IFQ_MAXLEN` = 50).

- **Output.** Output prepends the 2-byte type, where BSD prepends the address
  family, and appends the packet to the queue. A full queue drops the packet and
  counts it.
- **Deferred delivery.** Output then calls a kick hook. In the kernel this
  submits a worker item, so **delivery never happens inside output**. That
  avoids recursion and lock-order problems when a protocol replies to its own
  packet.
- **Service.** `nx_net_loop_service` delivers in FIFO order, up to the worker
  batch size per pass, and re-queues itself while packets remain.
- **Down and detach** free anything still queued.

## Live stack and em0

`net_start()` runs in process context. It:

- creates the packet pool from the [installed-memory limits](networking-pools.md);
- starts the worker, with the caller as its parent;
- attaches and brings up `lo0`;
- attaches `em0` (down) when [an e1000 is present](networking-pci.md).

`em0` is backed by the [e1000 driver](networking-e1000.md).

- **Up** starts the driver and copies the EEPROM MAC into the interface; until
  then the link address is zero.
- **Down** stops the driver.
- **Data path.** Received frames go to `nx_net_ether_input`, and output goes to
  `e1000_transmit`.

`net_stop()` detaches every interface (stopping the driver), stops the worker
and destroys the pool. The caller reaps the worker. Starting the stack during
normal boot belongs to task 429.

## Validation

`make test-net-if` runs ASan/UBSan fixtures with a fake Ethernet driver. It
covers:

- registry limits and name checks;
- protocol registration limits and duplicates;
- up/down with a failing driver;
- output while down, at the MTU boundaries, without a packet header, and when
  the driver refuses;
- the exact header layout;
- input bounds (13, 14, 1514 and 1515 bytes);
- foreign, broadcast and own destinations;
- unregistered types;
- a header split across buffers;
- loopback deferral and FIFO order, service budgets, the 16384-byte MTU, the
  queue limit, and flushing on down and detach.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-if-guest` (the same boots as `test-pci-guest`) starts and stops
the stack twice at 16 MiB and 128 MiB.

- **Loopback.** 55 packets are queued in a single worker pass. Nothing is
  delivered during that pass, 5 are dropped, and the other 50 then arrive intact
  with every mbuf freed.
- **em0** (when present):
  - output is refused while it's down and when the payload is above the MTU;
  - up reads the MAC;
  - an ARP request through Ethernet framing gets the gateway's reply,
    demultiplexed to the ARP protocol on `em0`;
  - down and up work again.
- **Shutdown.** Stopping leaves no interfaces, a stopped driver and every page
  returned.
- **No NIC.** Without an e1000 the boot reports `NET ETHERNET ABSENT`.

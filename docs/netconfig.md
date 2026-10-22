# Interface and route configuration (task 427)

Root configures [interfaces and static routes](networking-routing.md) with two
installed commands, `/etc/ifconfig` and `/etc/route`. They are native programs
built on configuration ioctls that any [socket](sockets.md) descriptor
accepts.

- **Queries** are open to every user.
- **Changes** need effective uid 0. Otherwise they fail with `EPERM`, which the
  commands report as `Not owner`.

Running either command opens a socket, so the first use starts the network
stack, with `lo0` at 127.0.0.1/8 and every other interface down and
unaddressed.

## Commands

```
ifconfig [-a]                                   list every interface
ifconfig interface                              show one
ifconfig interface [address[/bits]] [netmask mask] [up|down]
ifconfig interface delete [up|down]             remove the address
route [show]                                    connected routes and the default
route add default gateway
route delete default
route get destination                           where a destination goes
```

**Addresses and masks.** Addresses are strict dotted quads. A mask is `/bits`
(1–32), a dotted quad, or `0x` hex. An address without a mask gets BSD's
classful default: /8, /16 or /24.

**Changes are applied whole.** One `ifconfig` call is one kernel change. The
address is set first, then the interface is brought up or down. If bringing
it up fails, the previous address (or none) is restored and nothing has
changed.

**Errors.** Malformed text is rejected by the command before any request
(`bad address`, `bad prefix`, `bad netmask`, `bad gateway`, or the usage
message; exit status 2). Kernel refusals print `command: subject: message`
and exit 1:

| Refusal | Error |
|---|---|
| Unknown interface | `ENXIO` |
| Non-contiguous mask; non-unicast, network, broadcast or loopback-range address off `lo0` | `EINVAL` |
| Subnet overlapping another interface's | `EEXIST` |
| Gateway outside every configured non-loopback subnet | `ENETUNREACH` |
| Gateway that is one of our addresses | `EINVAL` |
| Driver failure on up (after rolling back) | `EIO` |

Output looks like this (counters vary):

```
em0: flags=43<UP,BROADCAST,RUNNING> mtu 1500
	ether 52:54:00:12:34:56
	inet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255
	packets in 12 out 9, errors in 0 out 0, drops 0

Destination     Gateway         Netmask         Interface
127.0.0.0       link            255.0.0.0       lo0
10.0.2.0        link            255.255.255.0   em0
default         10.0.2.2        0.0.0.0         em0

8.8.8.8 via 10.0.2.2 dev em0 src 10.0.2.15
```

Details:
- Loopback shows no `ether` line and no broadcast address.
- **Connected routes** are listed only for interfaces that are running and
  addressed, as routing uses them.
- **The default gateway** is rechecked at every lookup, so with its interface
  down it is shown with interface `-`.
- **`route get`** marks `local` (delivered through `lo0`) and `broadcast`
  destinations.

Configuration is not persistent. Task 429 adds the optional `/etc/netstart`
that applies it at boot.

## Socket ioctls

`<nexix/socket.h>` defines the requests and fixed records. Addresses are in
network byte order.

| Request | Record | Root | Effect |
|---|---|---|---|
| `NX_SIOCGIFINFO` 0x6901 | `nx_ifinfo` (96 bytes) | no | By name, or by `index` when the name is empty (`ENXIO` past the end): flags, MTU, link address, address/mask/broadcast and counters |
| `NX_SIOCSIFCONFIG` 0x6902 | `nx_ifconfig` (24) | yes | `NX_IFC_ADDRESS` or `NX_IFC_CLEAR`, and `NX_IFC_UP` or `NX_IFC_DOWN`, applied whole |
| `NX_SIOCGROUTE` 0x6903 | `nx_route` (8) | no | The default gateway; `NX_RT_DEFAULT` when one is set |
| `NX_SIOCSROUTE` 0x6904 | `nx_route` | yes | Set the default gateway, or clear it with 0 |
| `NX_SIOCLOOKUP` 0x6905 | `nx_route_lookup` (24) | no | Interface, next hop, source and `LOCAL`/`BROADCAST`/`GATEWAY` flags for a destination |

Validation:

- **Records:**
  - names must be NUL-terminated within 8 bytes;
  - an empty or unknown change set, both of a pair, a nonzero reserved field
    or nonzero route flags is `EINVAL`;
  - an unusable lookup destination (0/8, multicast, reserved) is `EINVAL`;
    no route is `ENETUNREACH`.
- **Pointers:** a record the kernel must write back must be writable, and one
  it reads must be readable (`EFAULT`).
- **Requests:** any other request on a socket, and these requests on other
  files, are `ENOTTY`.

`kernel/net/config.c` holds the worker-context logic behind the requests.

## Validation

- **`make test-net-config`** (ASan/UBSan) uses `lo0`, two fake Ethernet
  interfaces (one whose driver can refuse to come up) and a manual clock. It
  covers:
  - queries by index and by name, and unknown or unterminated names;
  - every malformed change, and invalid and overlapping addresses;
  - derived broadcasts, and none for loopback or /31;
  - rollback to the previous address, or to none, when up fails;
  - default-route validation, and lookups of local, connected, gateway,
    broadcast and refused destinations;
  - down and clear.
- **`make test-netconfig`** boots disposable guests at 16/128 MiB with an
  e1000, and at 16 MiB without one. As root it runs the installed commands and
  checks their exact output and status:
  - the initial `lo0` and route table, and every malformed-input and refusal
    message;
  - em0 refusing a broadcast or loopback address without changing;
  - addressing and bringing it up (flags, MAC, broadcast), readdressing, the
    classful default and a hex netmask;
  - the default route and its table, and `route get` for gateway,
    connected, local and broadcast destinations;
  - down (routes gone, gateway shown as `-`), delete, and clearing the default.

  The `tests/netconfig-api.c` fixture then checks `EFAULT`, `ENOTTY` (also on
  a pipe) and malformed records. It also checks that a uid-42 child may query
  but gets `EPERM` from both ioctls, that `ifconfig` run as that child prints
  `Not owner` and exits 1, and that the interface is left untouched. Without a
  NIC, `em0` is `ENXIO` and only `lo0` is listed.

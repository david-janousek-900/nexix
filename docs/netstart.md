# Network startup and the QEMU profile (task 429)

## Optional /etc/netstart

After `/etc/rc`, on every transition to multi-user mode, `init` checks for
`/etc/netstart`. If the file exists, `init` runs it with `/bin/sh`: standard
input from `/dev/null`, output and errors on the console. It waits for the
script to finish before starting logins.

**Without `/etc/netstart`, nothing is configured.** The network stack only
starts when a program first opens a socket, and then only `lo0` gets
127.0.0.1/8; external interfaces stay down and unaddressed. A fresh image
ships no `/etc/netstart`.

`init`, not `/etc/rc`, runs the script. Upgrades keep an existing `/etc/rc`,
so the hook works on upgraded disks too.

The installed sample `/etc/netstart.qemu` configures QEMU user networking.
Enable it with:

```sh
cp /etc/netstart.qemu /etc/netstart
```

It contains:

```sh
/sbin/ifconfig em0 10.0.2.15/24 up
/sbin/route add default 10.0.2.2
```

**Writing the script:**
- It is an ordinary V7 shell script, usually [`ifconfig` and `route`](netconfig.md)
  commands.
- The V7 shell has no `#` comments: use `: 'text'` lines.
- A failing command prints its error on the console, and boot carries on.
  For example, without a NIC you will see
  `ifconfig: em0: No such device or address`.

**Upgrades:** `/etc/netstart` is not part of the installation manifest, so
[upgrades](disk-upgrades.md) never replace or remove it. `/etc/netstart.qemu`
is configuration: it is installed when missing, and otherwise kept as it is.

## The QEMU network profile

`make run`, `make run-vga` and `make run-serial` take two variables:

| Variable | Values | Default | Effect |
|---|---|---|---|
| `NIC` | `user`, `none` | `user` | `user` attaches an e1000 on QEMU user networking (guest 10.0.2.15, gateway and host 10.0.2.2, DNS 10.0.2.3); `none` attaches no card |
| `HOSTFWD` | space-separated `tcp\|udp:hostport:guestport` | empty | Each entry forwards a host port to a guest port; `NIC=user` only |

```sh
make run-serial                               # e1000, no forwards
make run-serial HOSTFWD="tcp:8080:7000"       # host 8080 -> guest 7000
make run-serial NIC=none                      # no network card
make -s print-qemu-nic HOSTFWD="tcp:8080:7000 udp:5300:53"   # show the QEMU option
```

A malformed entry, an unknown `NIC`, or forwards with `NIC=none` stop make with
an explanation. Forwarded ports listen on all host addresses, as is usual for
QEMU's `hostfwd`.

For example, inside the guest:

```sh
nc -l 7000
```

On the host, `nc localhost 8080` then talks to it. Outbound guest connections
need no forwarding; they leave through QEMU's NAT.

## Validation

- **`make test-qemu-profile`** runs `tests/qemu_profile.py`: the default and
  `NIC=none` options, single and multiple forwards, five rejected profiles,
  forwards with `NIC=none`, and the wiring of every run target.
- **`make test-upgrade-unit`:** an administrator's `/etc/netstart` keeps its
  bytes, inode, mode, owner and times across an applied upgrade.
- **`make test-netstart`** cold-boots disposable multi-user guests at 16 and
  128 MiB and logs in as root:
  - **with `/etc/netstart`** (the shipped sample, enabled): `em0` comes up at
    10.0.2.15/24 with the default route, and QEMU's gateway answers a ping;
    the console shows no errors;
  - **without it** (the default image): `em0` has no address, the route table
    lists only `lo0`, and the sample is installed;
  - **with it but no NIC:** the console reports the `em0` error, and boot
    reaches login with only `lo0`;
  - **with a host forward:** a host client sends to a guest `nc` listener
    through the forwarded port, and a guest listener sends back to the host.

## Resolver profile (task 454)

DNS configuration is separately optional. After enabling netstart, enable
QEMU's DNS with `cp /etc/resolv.qemu /etc/resolv.conf`. The sample contains
`nameserver 10.0.2.3`. Its name is shortened from the planned
`resolv.conf.qemu` because V7 directory components allow only 14 bytes.
The default image has no `/etc/resolv.conf`; absence selects loopback DNS
and permits immediate hosts fallback when no local server is listening.

The profile and hosts/services/protocols/networks files are configuration:
upgrades install missing files and preserve existing bytes, ownership, modes
and timestamps. `/etc/resolv.conf` is administrator-owned and outside the
manifest; upgrades keep it too. Startup does not rewrite resolver files.
Tests verify the manifest/profile, preserved custom databases and resolv.conf,
and cold boots with optional network/DNS configuration at 16/128 MiB.

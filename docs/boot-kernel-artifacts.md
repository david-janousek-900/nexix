# Historical boot and kernel artifacts (task 351)

[The artifact map](boot-kernel-artifacts.json) accounts for the six inputs owned
by `distribution.boot-kernels`. Each record pins its size and SHA-256 to the
original [distribution index](../reference/distribution.json), identifies source
evidence for its historical role, and associates its role with native build
outputs and maintained implementation sources.

| Historical image | Role evidence | Native association |
|---|---|---|
| `/boot` | Standalone `boot.c`: read a named PDP-11 executable and transfer control | GRUB BIOS ISO, `boot/grub.cfg`, native Multiboot2 entry |
| `/unix` | Kernel link recipe in `usr/sys/conf/makefile` | `build/nexix.elf` |
| `/hphtunix` | `allsystems` recipe and `hphtconf` | Same configurable native kernel |
| `/hptmunix` | `allsystems` recipe and `hptmconf` | Same configurable native kernel |
| `/rphtunix` | `allsystems` recipe and `rphtconf` | Same configurable native kernel |
| `/rptmunix` | `allsystems` recipe and `rptmconf` | Same configurable native kernel |

These are role associations. They do not establish that a historical image was
reproduced from the supplied source revision, that it executes on x86_64, or
that all of its behavior has passed native acceptance. The original bytes stay
under `reference/v7` and must not be installed as guest programs. Native images
are built by the top-level Makefile; generated images are not provenance inputs
and need not exist to validate this map.

Task 351 completes provenance accounting only. Standalone runtime and tools
remain tasks 344–350, including disk/tape boot paths. `/usr/mdec` bootstrap
artifacts remain owned by `distribution.boot-support`. Kernel behavioral
integration and the unresolved freeze remain task 358; device/consumer closure
remains task 359. Neither an ISO association nor this audit closes those tasks.

Run `make test-boot-artifacts` to check exact ownership, archived hashes and
sizes, safe native/source paths, role mappings and rejection of false execution
claims. `test-inventory` also runs this check. No historical file is modified.

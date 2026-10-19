# Task 85: independent bootstrap and full IDE roots

The native build now has two installation profiles:

| Profile | Plan/configuration | Stage/image | Purpose |
| --- | --- | --- | --- |
| Bootstrap | `docs/bootstrap-install.json`, `rootfs/bootstrap` | `build/bootstrap-stage`, `build/root.img` | Fixed 512 KiB, 96-inode RAM recovery and kernel acceptance |
| Full installation | `docs/native-install.json`, `rootfs` | `build/stage`, `build/distribution.img` | Growing IDE distribution, including future multi-user programs/configuration |

Run `make bootstrap-rootfs` for the embedded root, `make rootfs` for the full
installation seed, or `make build` for the ISO and persistent working disk.
`make stage` verifies full staging; `make bootstrap-stage` verifies bootstrap
staging. Both profiles retain component provenance and payload hashes.

The full seed automatically sizes its filesystem with at least 16 MiB of free
blocks and 2,048 free inodes. Its capacity grows with installed payloads. These
budgets are build policy, not changes to V7 disk formats. The task-84 image
builder CLI supports explicit sizing for separately named custom images.

Only the bootstrap profile supplies embedded executables, the fallback root
image and their kernel dependencies. Full-only payloads and configuration are
absent from that build graph: the kernel can build even when a full-only input
has not been produced. Existing bootstrap executable sources remain shared with
native builds; changes to those selected programs can still affect the kernel.
The inventory audit checks profile ownership and required init/shell membership.
Full installation entries cannot carry bootstrap-selection flags.

The fixed bootstrap retains its own rc/ttys/passwd/profile inputs, so changing
multi-user account/startup configuration does not change RAM recovery behavior.
Its explicit 512 KiB/96-inode build limits reject accidental growth. Adding a
program to the full installation requires no bootstrap manifest change.

## Boot and disk lifetime

`make run` continues to need no variable overrides. It attaches `build/nexix.img`
and boots the ISO. A new working disk receives the full distribution seed;
an existing path is never replaced or resized. Existing installations are not
silently upgraded. Use [explicit offline upgrades](disk-upgrades.md) to update an
existing working disk with a backup. To use a fresh installation while preserving the old disk:

```
make DISK_IMAGE=build/fresh.img run
```

`tools/seed_disk.py` streams the seed into a temporary sibling, flushes it and
publishes it using an exclusive hard link. Concurrent creators cannot overwrite
one another. Copy/publication failure leaves no partially initialized working
disk. Existing paths, including symlinks, are left untouched. The seed and working
disk are independent files after publication.

The kernel's existing root-device path mounts the primary IDE filesystem directly
rather than copying it into RAM. If that device is absent, it mounts the fixed
embedded image. A present but invalid/unreadable IDE root fails; it does not hide
storage errors by falling back to RAM. The same kernel/ISO boots either profile.
The ISO still supplies GRUB; the raw IDE image has no partition table or bootloader.

V7 reference contracts are `usr/sys/sys/main.c` (`main`/`iinit` root mounting),
`usr/sys/conf/c.c` and `mkconf.c` (`rootdev`), and `usr/src/cmd/mkfs.c` (external
filesystem population). Native GRUB/ELF and IDE mechanisms replace the historical
boot/device machinery. Separate mkconf, guest mkfs and standalone boot ports
remain with their own roadmap tasks. No reference sources are modified.

## Acceptance

`make test-root-separation` checks that adding an 8 MiB full-only payload and
changing full startup configuration leaves kernel and bootstrap image hashes
unchanged. It also proves that missing full-only inputs do not block `make all`.
Host tests cover profile drift, fixed bootstrap layout, new-disk creation,
existing-disk preservation, concurrent creators and copy/publication failures.

QEMU acceptance runs at 16/128 MiB with a disposable expanded installation. Each
run boots RAM recovery, then boots the full IDE root twice, reads full-only data
and configuration, executes an ELF through a full-only hard-link path absent
from the embedded catalog, and verifies cold-boot persistence. Normal shell and
storage tests now use the generated full distribution for IDE fixtures while RAM
boots continue exercising the fixed bootstrap. Existing disks are never used as
acceptance fixtures. Multi-user startup itself remains tasks 94–108.

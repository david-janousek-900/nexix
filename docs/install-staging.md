# Task 83: manifest-driven installation staging

`docs/native-install.json` is the native installation plan. Each entry names its
V7 inventory component, guest path, kind, mode, UID and GID. Regular files name
their build/configuration source and format; empty files, directories, devices
and hard links have explicit entries. Component IDs resolve through
`docs/v7-userland-manifest.json`, which owns the preserved reference sources.
The installation plan describes current native outputs, not all planned ports.

GNU Make derives installed ELF targets and command names from this plan.
Native executable artifacts currently use `build/v7-NAME.elf`; component-specific
recipes can produce them. Entries marked `bootstrap` also generate the embedded
catalog and image list used by existing kernel acceptance fixtures. Future
installed programs need not be embedded. Script and data payloads use the data
format with the required guest execute permissions; they are not mistaken for
ELF executables. The inventory audit checks implemented program claims against
the same plan rather than parsing independent hard-coded lists.

Run:

```
make stage
make rootfs
make test-install
```

`make stage` verifies the staged payloads even when build inputs are up to date.
`build/stage/root` contains staged regular payloads and directories.
`build/stage/receipt.json` records every guest object, its component and source,
file SHA-256 hashes, and hashes of the installation and reference inventories.
The receipt is deterministic and contains no host timestamps or absolute source
paths. Character/block nodes and hard-link aliases remain receipt metadata;
creating a stage requires neither root privileges nor host device nodes.
Guest ownership and set-ID bits are applied to the V7 image, not the host tree.
Host modes retain owner read/write/search access needed for staging.

The root-image builder validates the receipt and all staged payloads before
constructing guest inodes. Modes, ownership, device numbers and hard-link counts
come from the manifest. Stage paths, duplicate destinations, missing parents,
unsafe source paths, unsupported formats and invalid metadata are rejected.
ELF entries must match implemented inventory outputs and native ELF64 identity.

Rebuilding stages inputs into a temporary sibling tree before replacing the
existing stage. A missing or invalid input leaves the old stage intact. An
existing destination must be a verified generated stage: modified payloads,
permissions, unexpected files and symlinks cause refusal instead of deletion.
Only verified previous generated outputs are removed when a new plan drops an
entry. Keep local edits in source/configuration inputs; if a stage has local
changes worth retaining, move it aside before rebuilding. No staging command
writes the persistent working disk `build/nexix.img`.

Validation includes transactional failure cases, symlink/path rejection,
provenance and receipt tampering, stale-file removal, repeatable receipts,
UID/GID/set-ID propagation, hard links and guest devices. Direct and staged image
construction must match, and the default image remains byte-identical to its
pre-task layout. QEMU root acceptance runs at 16/128 MiB; shell and persistent
storage regressions protect boot behavior and user data.

[Task 84](image-builder.md) supplies scalable image capacity; task 85 owns separating the full
IDE distribution from the fixed embedded bootstrap root. Independent terminal
and multi-user implementation tasks follow those prerequisites. Task 83 alone
does not install getty/login or enable multi-user boot.

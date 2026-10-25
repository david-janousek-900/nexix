# Historical reference distributions

Baseline: [TUHS Henry Spencer V7 distribution](https://www.tuhs.org/Archive/Distributions/Research/Henry_Spencer_v7/v7.tar.gz).

Archive SHA-256:
`e0fd7bdd046e17de177f5d997acf8123fa43c57b46b032c7337f7666ab69a07e`

`v7/` retains all **2,144 archived files**, including kernel/userland sources,
installed shell scripts, historical binaries, headers, libraries, manual pages,
documents, dictionaries, lessons, configuration and game data. The earlier
import retained only usr/src and usr/sys; step 21 restores the other 910 files.
Existing imported contents were verified unchanged against the same archive.

[distribution.json](distribution.json) records each archive member's path,
SHA-256, size, original type, mode, UID/GID, timestamp and hard-link target.
Git preserves contents as regular files, including the hard-linked pk.p header;
the original link relationship and filesystem metadata are recorded in the index
rather than relying on host ownership, timestamps or Git checkout permissions.

Preserve the reference unchanged. Implement native ports in separate active
source directories. Historical executable/object bytes are provenance artifacts;
their presence does not make them runnable native Nexix programs. Distribution
accounts, spool messages and configuration are historical fixtures, not new
Nexix installation defaults.

To verify the repository against the original archive:

```sh
python3 tools/import_v7_reference.py /path/to/v7.tar.gz --check
make TOOLCHAIN_PREFIX= test-inventory
```

Run these commands from the repository root. The importer requires the pinned
archive hash, verifies safe member paths, and refuses to overwrite reference
files with differing contents. Without --check it fills missing files and writes
the provenance index. Inventory checks require no download and validate all local
contents against that committed index.

The historical UNIX license is retained as `Caldera-license.pdf` alongside this
file. Its terms apply to the historical distribution; do not assume that
provenance grants a license for unrelated additions.

## Complete 4.4BSD-Lite2 reference

`bsd-lite2/source/` preserves the entire
[4.4BSD-Lite2 mirror](https://github.com/sergev/4.4BSD-Lite2) at commit
`50587b00e922225c62f1706266587f435898126d`, including kernel, libraries,
commands, contributed software, manuals, configuration and Domestic/Foreign
source trees. This is a complete pinned mirror snapshot, not a claim of
independently authenticated original CSRG release media.

Archive SHA-256:
`5a851582c4c3a5053ef2f4f41df347996969a95c3c7236b0b9e99319ff02c3ef`

The import retains **19,228 regular files byte for byte** and **1,322 symbolic
links as inert regular files containing their original link-target text without
a newline**. No historical link is followed or recreated as a host filesystem
link. `bsd-lite2/provenance.json` records all 20,550 file/link entries and 1,639
directories, including original modes, ownership, timestamps, link types and
targets. Empty directories are recorded in the index and recreated on import;
Git does not preserve empty directories. Historical modes and ownership are
metadata, not required checkout permissions. Full-manifest and per-file hashes
are checked offline, separately from the V7 distribution index.

Existing source paths and the earlier 167 selected source files are unchanged.
`bsd-lite2/audit.json` remains the focused networking/layout/resolver include
and task-ownership audit; it is not an audit of every newly retained component.
See [the networking audit](../docs/bsd-networking-audit.md) and
[the kernel analysis](../docs/kernel-evolution.md). Importing reference sources
does not build them or complete native porting tasks.

The original `COPYRIGHT` and individual source/license notices are unchanged.
The retained Berkeley license amendment document is separately hash-pinned.
Contributed code has its own notices; the distribution-level Berkeley notice
is not a blanket license for every contributed file.

Verify locally without downloading:

```sh
make TOOLCHAIN_PREFIX= test-bsd-reference
make TOOLCHAIN_PREFIX= test-inventory
```

Compare against the pinned full archive, or fill missing files:

```sh
python3 tools/bsd_reference.py /path/to/bsd-lite2.tar.gz
python3 tools/bsd_reference.py /path/to/bsd-lite2.tar.gz --import-reference
```

The importer verifies the archive hash before reading members, rejects unsafe
or duplicate paths and unsupported types, and preflights existing files before
writing. It refuses to overwrite differing reference bytes. Preserve this tree
unchanged and place active native ports elsewhere.

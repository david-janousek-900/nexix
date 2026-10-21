# UNIX V7 reference distribution

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

## BSD networking reference

`bsd-lite2/source/` separately preserves 102 selected networking/support files
from a pinned 4.4BSD-Lite2 mirror snapshot. Its provenance index records the
immutable commit, full archive SHA-256 and per-file hashes. The retained
Berkeley license document is separately hash-pinned. These sources are not
part of the V7 archive and are not compiled into Nexix yet.
See the [baseline and dependency/LP64 audit](../docs/bsd-networking-audit.md).
Run `make test-bsd-reference` for offline integrity and rejection tests.

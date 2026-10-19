# License scope and third-party notices

Audit date: 2026-10-19. The root [LICENSE](LICENSE) is BSD-3-Clause for original
Nexix contributions by David Janousek. It does not replace upstream licenses or
grant rights to third-party works. In mixed files, original contributions are
BSD-3-Clause subject to the inherited code's conditions; CDDL-covered
modifications remain CDDL. File-specific notices take precedence.

## Historical UNIX: Caldera

The preserved V7 distribution in `reference/v7/`, the V6 supplemental inputs,
and their derived code/data retain the Caldera grant. Active examples include
`user/v7/`, `libm/v7/`, original shell scripts, and V7-derived portions of libc,
kernel code and headers. Moving or adapting code does not remove these terms.

The authoritative grant is [reference/Caldera-license.pdf](reference/Caldera-license.pdf),
also available from [TUHS](https://www.tuhs.org/Archive/Caldera-license.pdf).
[LICENSES/Caldera.txt](LICENSES/Caldera.txt) is a text extraction of the retained
PDF. SPDX identifies this license as [Caldera](https://spdx.org/licenses/Caldera.html),
not BSD-3-Clause. Its grant covers the specified historical UNIX versions and
explicitly excludes System III, System V and successors.

Source distributions must retain its copyright, conditions and disclaimer;
binary distributions must reproduce them in accompanying materials. Advertising
that mentions features or use must display this acknowledgement:

> This product includes software developed or owned by Caldera International, Inc.

The license also prohibits using contributor names for endorsement without
permission. BSD-3-Clause contributions can coexist with these terms, but cannot
remove the advertising condition. A blanket BSD-3-Clause claim for all repository
contents would be incorrect.

## Ratfor lex skeleton: CDDL-1.0

`reference/supplemental/illumos/nrform` and its active adaptation
`user/v7/lex/nrform` are Copyright (c) 1993 Sun Microsystems, Inc., under
**CDDL-1.0 only**. The active file also credits the 2026 Nexix modifications.
Both retain their CDDL headers. The full license is preserved in
[reference/supplemental/illumos/OPENSOLARIS.LICENSE](reference/supplemental/illumos/OPENSOLARIS.LICENSE)
and copied to [LICENSES/CDDL-1.0.txt](LICENSES/CDDL-1.0.txt).
The [pinned upstream license](https://github.com/illumos/illumos-gate/blob/966c588f828c4693b5a80ec27b24ff16afa11ec9/usr/src/OPENSOLARIS.LICENSE)
is the provenance source.

CDDL sections 3.1–3.4 require availability of covered source, preservation of
notices and license terms for covered modifications and executable distributions.
Section 3.6 permits larger works containing separately licensed code; it does
not permit relicensing the covered file as BSD. This skeleton is not installed
in the current default guest image. Future lex/Ratfor distribution must continue
to satisfy its source and notice requirements.

## Archived documents and historical binaries

The Caldera grant must not be treated as permission from unrelated rights
holders. In particular, `reference/v7/usr/doc/cacm/p1` identifies the article
“The UNIX Time-Sharing System” as Copyright 1974 Association for Computing
Machinery, Inc., “reprinted by permission.” That notice is preserved. The archive
does not establish a new general redistribution grant from ACM; resolve that
permission before redistributing the article as part of a public release.
`reference/v7/usr/doc/cman` is a historical placeholder explicitly stating that
the C Reference Manual source was withheld for copyright reasons.

Historical executables and objects are retained as provenance artifacts, not
native installed programs. Their presence and recorded hashes do not establish
source availability or additional third-party rights. Existing source/provenance
blockers remain in the [inventory](docs/porting-inventory.md) and
[release tasks](docs/tasks.md). The reference archive was not rewritten or
relicensed by this change.

## GRUB in generated boot ISOs

GRUB is a separate component under [GPL-3.0-or-later](https://www.gnu.org/software/grub/).
`grub-mkrescue` embeds host-provided GRUB binaries/modules in `build/nexix.iso`.
Using GPL build tools does not relicense Nexix source; distributing the embedded
GRUB binaries does carry GPL obligations for those components, including the
appropriate license/notices and corresponding source provision. Any additional
assets supplied by the host GRUB package retain their own notices as well.

The source repository does not vendor GRUB, and the current ISO recipe does not
bundle GRUB's corresponding source or a complete redistribution notice package.
Before publishing boot ISOs, identify the exact host GRUB package and patches,
include its required notices/licenses, and arrange compliant corresponding-source
access. Release packaging/provenance tasks 356 and 360 remain open; adding a
BSD LICENSE does not complete those release gates. GCC, Python, QEMU and other
host tools retain their own licenses and are not included merely by running them.

## Audit boundary

This review checked the retained Caldera grant, supplemental-input provenance,
active source notices, the CDDL skeleton, archived copyright notices and the
GRUB boot-image dependency. It found no basis to relicense Caldera, CDDL or ACM
material under BSD-3-Clause. The identified permissions and ISO packaging gaps
are documented above rather than asserted to be cleared. Hash/inventory checks
verify preservation and task mappings; they do not determine copyright ownership.

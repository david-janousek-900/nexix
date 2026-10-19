# Task 357: offline installation and upgrades

Nexix upgrades its whole-volume raw V7 IDE images on the host. Stop QEMU/libvirt
first. The tool takes an exclusive POSIX byte-range lock that conflicts with
QEMU's image locks; it refuses an active or corrupt image. Run as the image's
owner with read/write access. No host device nodes or root privileges are needed.

## Install a new disk

```sh
make DISK_IMAGE=build/new-install.img install-disk
```

This builds the distribution seed and exclusively publishes a new working disk.
An existing filename is preserved. It is the same installation primitive used by
`make build`/`make run`; it never implicitly upgrades an existing image.

## Preview and apply an upgrade

```sh
make DISK_IMAGE=build/nexix.img upgrade-preview
make DISK_IMAGE=build/nexix.img UPGRADE_BACKUP=build/nexix-before-upgrade.img upgrade-disk
```

The first command builds/verifies staging and prints a JSON plan without changing
the image. The second requires the VM to remain stopped, creates a complete
mode-0600 backup, builds and verifies a candidate image, then atomically replaces
the working image. An existing backup path is never overwritten; choose a new
backup name for each changed installation. A repeat upgrade with no changes is
a no-op. Backups include the original image bytes, accounts and data. The report
includes the backup's SHA-256. Store valuable backups outside `build/` before
`make clean`, which deletes that directory.

To invoke the tool directly:

```sh
python3 tools/upgrade_disk.py --image build/nexix.img --stage build/stage
python3 tools/upgrade_disk.py --image build/nexix.img --stage build/stage --apply --backup build/nexix-before-upgrade.img
```

The direct command defaults to a preview too. `--manifest` can select a verified
installation manifest. Stage receipt, source hashes, paths and payloads are
checked by the existing installation tools before the disk is read.

## Preservation policy

- Files sourced from built programs, libraries and headers, plus installed
  executable scripts, are managed payloads and are replaced when their contents
  or packaged metadata differ. Review these destinations in the preview.
- Existing configuration files, including passwd/group, rc/ttys, profiles, TZ,
  passwords and accounting/session files, retain contents, owners, modes and
  all three V7 timestamps. Missing defaults are installed. New defaults for
  existing configuration can be reviewed in `build/stage/root`; no automatic
  merge changes account IDs, password hashes, home paths or services.
- Other files and directories remain in the namespace, including unlisted old
  programs. Ordinary hard links, arbitrary V7 filename bytes, sparse file holes,
  device numbers and file metadata are preserved. A user hard link to an old
  managed binary keeps the old bytes; the packaged path gets the new inode.
- Unchanged inode numbers and timestamps remain intact. New/replaced objects get
  the upgrade time, and directories whose entries change get updated mtime/ctime.
  All disk addresses, counts and timestamps retain V7 PDP-11 encodings.
- The image retains at least its former volume capacity and grows when necessary
  for installed payloads and free space. Existing device nodes and directory
  metadata are preserved; mismatched object types/device numbers cause refusal.

In particular, upgrading a disk where root's passwd home is still `/` preserves
that account field and its profile/TZ. Review the new `/root/.profile` and migrate
the account explicitly if desired; fresh installations already use `/root`.
The updater does not remove obsolete paths or guess how local configuration
should be merged. It also does not install a disk bootloader; retain the ISO.

## Transaction boundaries and rejected inputs

The reader checks inode/data-block ownership, sparse direct/indirect addressing,
namespace/link counts, directory parents and free-block/free-inode accounting.
Malformed images, allocated orphan inodes, directory cycles/hard links, nonzero
bad-block inode 1, partitioned/trailing-data images, host symlink/hard-linked
image paths and modified staging are rejected. Recover a damaged filesystem
before attempting an upgrade; this is not a filesystem repair tool.

Backups are published exclusively and synced before candidate construction.
Candidate failures leave the working image untouched and retain the backup.
The candidate is fully checked before publication, and image identity/content
are checked again under the lock. Publication is one rename with file/directory
syncs. After publication, a host I/O failure during the final directory sync can
still be reported; the complete backup remains available for recovery. Keep the
VM stopped while restoring a backup to the working path.

`make test-upgrade-unit` checks preservation (including three-level sparse files
and non-ASCII names), active locks, corruption, tampering, type conflicts, backup
collisions and simulated candidate/publication failures. `make test-upgrade`
upgrades customized production-seed copies at 16 and 128 MiB, then boots each
twice to verify binary replacement, accounts, TZ, hard links and persistent data.
Evidence is saved in `build/upgrade-*`; tests never attach the working disk.

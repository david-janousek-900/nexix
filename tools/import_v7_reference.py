"""Import or check the pinned Henry Spencer V7 distribution without executing it."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_SHA256 = 'e0fd7bdd046e17de177f5d997acf8123fa43c57b46b032c7337f7666ab69a07e'
ARCHIVE_URL = 'https://www.tuhs.org/Archive/Distributions/Research/Henry_Spencer_v7/v7.tar.gz'


def archive_index(archive):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise ValueError('archive SHA-256 does not match the pinned V7 distribution')
    records, contents = [], {}
    with tarfile.open(archive, 'r:gz') as tar:
        for member in tar.getmembers():
            path = PurePosixPath(member.name)
            if path.is_absolute() or '..' in path.parts or str(path) != member.name:
                raise ValueError(f'unsafe archive path: {member.name}')
            if member.name in contents or not (member.isfile() or member.islnk()):
                raise ValueError(f'duplicate or unsupported archive member: {member.name}')
            if member.islnk():
                target = PurePosixPath(member.linkname)
                if target.is_absolute() or '..' in target.parts:
                    raise ValueError(f'unsafe hard-link target: {member.linkname}')
            data = tar.extractfile(member).read()
            contents[member.name] = data
            records.append(dict(path=member.name, type='hardlink' if member.islnk() else 'file',
                                link_target=member.linkname if member.islnk() else None,
                                size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                                mode=member.mode, uid=member.uid, gid=member.gid, mtime=member.mtime))
    index = dict(schema_version=1, archive_url=ARCHIVE_URL, archive_sha256=ARCHIVE_SHA256,
                 reference_root='reference/v7', link_policy='Git records hard-link contents as regular files; original link targets are retained here.',
                 files=sorted(records, key=lambda record: record['path']))
    return index, contents


def check_reference(index, root=ROOT):
    errors = []
    if not isinstance(index, dict) or index.get('schema_version') != 1 or index.get('archive_sha256') != ARCHIVE_SHA256 or index.get('reference_root') != 'reference/v7':
        return ['invalid V7 archive provenance']
    expected = set()
    reference = root / 'reference/v7'
    for record in index.get('files', []):
        name = record['path']
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or str(path) != name or name in expected:
            errors.append(f'unsafe or duplicate provenance path: {name}')
            continue
        expected.add(name)
        file = reference / name
        if file.is_symlink() or not file.is_file():
            errors.append(f'missing/non-regular archive file: {name}')
            continue
        data = file.read_bytes()
        if len(data) != record['size'] or hashlib.sha256(data).hexdigest() != record['sha256']:
            errors.append(f'archive content mismatch: {name}')
        if record['type'] not in {'file', 'hardlink'}:
            errors.append(f'unsupported provenance type: {name}')
    actual = {str(file.relative_to(reference)) for file in reference.rglob('*') if file.is_file() or file.is_symlink()}
    for name in sorted(actual - expected):
        errors.append(f'unrecorded reference file: {name}')
    if len(expected) != 2144:
        errors.append(f'archive coverage mismatch: expected 2144 files, recorded {len(expected)}')
    for record in index.get('files', []):
        if record['type'] == 'hardlink' and record['link_target'] not in expected:
            errors.append(f'missing hard-link target: {record["path"]}')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--check', action='store_true', help='Verify archive, provenance and reference without changing files')
    args = parser.parse_args()
    try:
        index, contents = archive_index(args.archive)
        ledger = ROOT / 'reference/distribution.json'
        if args.check:
            if json.loads(ledger.read_text()) != index:
                raise ValueError('provenance index differs from the pinned archive')
            errors = check_reference(index)
            if errors:
                raise ValueError('\n'.join(errors))
        else:
            # Preflight all existing paths before adding anything; never overwrite edits.
            reference = ROOT / 'reference/v7'
            for name, data in contents.items():
                file = reference / name
                if file.is_symlink() or any(parent.is_symlink() for parent in file.parents if parent != ROOT):
                    raise ValueError(f'symlink in destination path: {name}')
                if file.exists() and (not file.is_file() or file.read_bytes() != data):
                    raise ValueError(f'existing reference differs: {name}')
            for name, data in contents.items():
                file = reference / name
                if not file.exists():
                    file.parent.mkdir(parents=True, exist_ok=True)
                    file.write_bytes(data)
            ledger.write_text(json.dumps(index, indent=2) + '\n')
        print(f'PASS: pinned V7 archive and {len(index["files"])} reference files' + (' verified' if args.check else ' imported'))
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        sys.exit(f'FAIL: {error}')


if __name__ == '__main__':
    main()

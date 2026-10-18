"""Read V7 PDP-11 ar files without host ar or executing archived content."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def longword(data, offset):
    high, low = struct.unpack_from('<HH', data, offset)
    return (high << 16) | low


def members(data):
    if data[:2] != b'\x65\xff':
        raise ValueError('not a V7 archive')
    offset, names, result = 2, set(), []
    while offset < len(data):
        if len(data) - offset < 26:
            raise ValueError('truncated archive header')
        header = data[offset:offset + 26]
        raw = header[:14].split(b'\0', 1)[0]
        try:
            name = raw.decode('ascii')
        except UnicodeDecodeError as error:
            raise ValueError('non-ASCII archive name') from error
        if not name or name in {'.', '..'} or '/' in name or '\\' in name or name in names:
            raise ValueError('unsafe or duplicate archive name')
        if any(c < 32 or c == 127 for c in raw):
            raise ValueError('control character in archive name')
        names.add(name)
        size = longword(header, 22)
        start, end = offset + 26, offset + 26 + size
        padded = end + (size & 1)
        if padded > len(data):
            raise ValueError('truncated archive payload or alignment')
        payload = data[start:end]
        result.append((dict(name=name, size=size, mtime=longword(header, 14),
                            uid=header[18], gid=header[19],
                            mode=struct.unpack_from('<H', header, 20)[0],
                            sha256=hashlib.sha256(payload).hexdigest()), payload))
        offset = padded
    return result


def extract(source, destination):
    source, destination = Path(source), Path(destination)
    parsed = members(source.read_bytes())  # Validate the whole input before writing.
    if destination.exists():
        raise ValueError('extraction destination already exists')
    destination.mkdir(parents=True)
    for record, payload in parsed:
        (destination / record['name']).write_bytes(payload)
    # Payloads remain non-executable. Historical modes/dates are provenance only.
    return [record for record, _ in parsed]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/v7-archives')
    args = parser.parse_args()
    inventory = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())
    sources = inventory['historical_source_archives'] + inventory['data_archives']
    records = []
    # Preflight every archive before publishing any output.
    for name in sources:
        source = ROOT / name
        parsed = members(source.read_bytes())
        records.append(dict(source=name, sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                            members=[r for r, _ in parsed]))
    if args.output.exists():
        raise SystemExit('output already exists; choose a fresh --output directory')
    args.output.mkdir(parents=True)
    for record in records:
        extract(ROOT / record['source'], args.output / Path(record['source']).name)
    (args.output / 'provenance.json').write_text(json.dumps(records, indent=2) + '\n')
    print(f'Extracted {len(records)} V7 archives; {sum(len(r["members"]) for r in records)} members')


if __name__ == '__main__':
    main()

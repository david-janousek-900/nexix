"""Package the native installation manifest into a deterministic V7 root volume."""
import argparse
from pathlib import Path
from mk_v7fs import Image, DIR, REG, CHR, BLK
from install_manifest import MANIFEST, load, materialize, read_stage


def build(blocks, programs, config, manifest=MANIFEST, stage=None):
    records, payloads = (read_stage(stage, manifest) if stage is not None else
                         materialize(load(manifest), programs, config))
    image = Image(blocks, inode_blocks=12)  # Full-release capacity belongs to task 84.
    numbers = {}
    number = 2
    for entry in records:
        path = entry['path']
        if entry['type'] == 'link':
            numbers[path] = numbers[entry['target']]
        else:
            numbers[path] = number
            number += 1
    if number-1 > (image.isize-2)*8:
        raise ValueError('installation does not fit inode area')
    kinds = {'file': REG, 'directory': DIR, 'character': CHR, 'block': BLK}
    for e in records:
        if e['type'] in ('directory', 'link'):
            continue
        ino = numbers[e['path']]
        image.inode(ino, kinds[e['type']] | int(e['mode'], 8), payloads.get(e['path'], b''),
                    device=e.get('device', 0), nlink=sum(n == ino for n in numbers.values()),
                    uid=e['uid'], gid=e['gid'])
    pending = [e for e in records if e['type'] == 'directory']
    while pending:
        # Stable child-before-parent order preserves existing fixture disk bytes.
        e = next(e for e in pending if not any(
            other['path'] != e['path'] and str(Path(other['path']).parent) == e['path']
            for other in pending))
        path = e['path']
        parent = str(Path(path).parent)
        children = [(Path(child).name, ino) for child, ino in numbers.items()
                    if child != '/' and str(Path(child).parent) == path]
        image.directory(numbers[path], numbers[parent], sorted(children),
                        mode=int(e['mode'], 8), uid=e['uid'], gid=e['gid'])
        pending.remove(e)
    return image.finish()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blocks', type=int, default=1024)
    parser.add_argument('--programs', type=Path, default=Path('build'))
    parser.add_argument('--config', type=Path, default=Path('rootfs'))
    parser.add_argument('--manifest', type=Path, default=MANIFEST)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        data = build(args.blocks, args.programs, args.config, args.manifest, args.stage)
        args.output.write_bytes(data)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    print(f'Built {args.output}: {len(data)//512} V7 blocks from {args.manifest.name}')


if __name__ == '__main__':
    main()

"""Package the native installation manifest into a deterministic V7 root volume."""
import argparse
from collections import Counter, defaultdict
import heapq
from pathlib import Path
from mk_v7fs import Image, DIR, REG, CHR, BLK, MAX_BLOCKS, MAX_INODE_BLOCKS, file_blocks, write_image
from install_manifest import MANIFEST, load, materialize, read_stage


def build(blocks, programs, config, manifest=MANIFEST, stage=None, inode_blocks=None, spare_inodes=24, spare_blocks=None):
    records, payloads = (read_stage(stage, manifest) if stage is not None else
                         materialize(load(manifest), programs, config))
    numbers = {}
    number = 2
    for entry in records:
        path = entry['path']
        if entry['type'] == 'link':
            numbers[path] = numbers[entry['target']]
        else:
            numbers[path] = number
            number += 1
    if type(spare_inodes) is not int or spare_inodes<0:
        raise ValueError('invalid spare inode count')
    if inode_blocks is None:
        inode_blocks=max(12,(number-1+spare_inodes+7)//8)
    if type(inode_blocks) is not int or not 1<=inode_blocks<=MAX_INODE_BLOCKS:
        raise ValueError('invalid inode area')
    if number-1>inode_blocks*8:
        raise ValueError('installation does not fit inode area')
    children=defaultdict(list)
    directories={e['path']:e for e in records if e['type']=='directory'}
    order={e['path']:i for i,e in enumerate(records)}
    remaining=Counter()
    for path,ino in numbers.items():
        if path!='/':
            parent=str(Path(path).parent)
            children[parent].append((Path(path).name,ino))
            if path in directories:remaining[parent]+=1
    required=2+inode_blocks
    for e in records:
        if e['type']=='file':required+=file_blocks(len(payloads[e['path']]))
        elif e['type']=='directory':required+=file_blocks(16*(2+len(children[e['path']])))
    if spare_blocks is not None and (type(spare_blocks) is not int or spare_blocks<0):
        raise ValueError('invalid spare block count')
    if blocks is None:
        blocks=max(256,required+(max(128,(required+3)//4) if spare_blocks is None else spare_blocks))
    if type(blocks) is not int or not 256<=blocks<=MAX_BLOCKS:
        raise ValueError('volume must have 256..16777216 blocks')
    if required>blocks or (spare_blocks is not None and blocks-required<spare_blocks):
        raise ValueError('installation does not fit volume/free-space budget')
    image = Image(blocks, inode_blocks=inode_blocks)
    links=Counter(numbers.values())
    kinds = {'file': REG, 'directory': DIR, 'character': CHR, 'block': BLK}
    for e in records:
        if e['type'] in ('directory', 'link'):
            continue
        ino = numbers[e['path']]
        image.inode(ino, kinds[e['type']] | int(e['mode'], 8), payloads.get(e['path'], b''),
                    device=e.get('device', 0), nlink=links[ino],
                    uid=e['uid'], gid=e['gid'])
    # Stable topological order keeps old disk bytes without cubic directory scans.
    ready=[(order[path],path) for path in directories if not remaining[path]]
    heapq.heapify(ready)
    while ready:
        _,path=heapq.heappop(ready)
        e=directories[path];parent=str(Path(path).parent)
        image.directory(numbers[path],numbers[parent],sorted(children[path]),
                        mode=int(e['mode'],8),uid=e['uid'],gid=e['gid'])
        if path!='/':
            remaining[parent]-=1
            if not remaining[parent]:heapq.heappush(ready,(order[parent],parent))
    return image.finish()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blocks', type=lambda v: None if v=='auto' else int(v), default=1024)
    parser.add_argument('--inodes', type=int, help='inode slots, rounded up to eight; default automatic')
    parser.add_argument('--spare-inodes', type=int, default=24, help='minimum automatic free inode budget')
    parser.add_argument('--spare-blocks', type=int, help='minimum free blocks; default auto size adds 25%% or 128 blocks')
    parser.add_argument('--programs', type=Path, default=Path('build'))
    parser.add_argument('--config', type=Path, default=Path('rootfs'))
    parser.add_argument('--manifest', type=Path, default=MANIFEST)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.inodes is not None and args.inodes<1:
            raise ValueError('invalid inode count')
        inode_blocks=None if args.inodes is None else (args.inodes+7)//8
        data = build(args.blocks, args.programs, args.config, args.manifest, args.stage,
                     inode_blocks,args.spare_inodes,args.spare_blocks)
        write_image(args.output,data)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    print(f'Built {args.output}: {len(data)//512} V7 blocks from {args.manifest.name}')


if __name__ == '__main__':
    main()

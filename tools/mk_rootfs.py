"""Package Nexix 0.1.0 native userland into a deterministic V7 root volume."""
import argparse
from pathlib import Path
from mk_v7fs import Image, DIR, REG, CHR

COMMANDS = ('sh', 'echo', 'cat', 'ls', 'pwd', 'cp', 'ln', 'rm')

def build(blocks, programs, config):
    image = Image(blocks)
    # Fixed inode assignments preserve reproducibility and leave spare inodes.
    entries = {
        '/': 2, '/bin': 3, '/etc': 4, '/dev': 5, '/tmp': 6,
        '/usr': 7, '/usr/adm': 8,
    }
    files = [('/etc/init', programs / 'v7-init.elf', 0o755)]
    files += [('/bin/' + name, programs / ('v7-' + name + '.elf'), 0o755)
              for name in COMMANDS]
    files += [('/etc/' + name, config / 'etc' / name, 0o644)
              for name in ('rc', 'ttys', 'passwd')]
    files += [('/.profile', config / 'profile', 0o644)]
    number = 9
    for path, source, mode in files:
        data = source.read_bytes()
        if mode & 0o111 and (len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01' or
                            data[16:20] != b'\x02\x00\x3e\x00'):
            raise ValueError(f'{source}: expected native x86_64 ELF64')
        entries[path] = number
        image.inode(number, REG | mode, data)
        number += 1
    for path in ('/etc/utmp', '/usr/adm/wtmp'):
        entries[path] = number
        image.inode(number, REG | 0o644)
        number += 1
    entries['/dev/console'] = entries['/dev/ttyS0'] = number
    image.inode(number, CHR | 0o600, device=0x100, nlink=2)
    number += 1
    entries['/dev/tty'] = number
    image.inode(number, CHR | 0o600, device=0x200)
    # Children before parents: directory link counts depend on inode types.
    for path in ('/bin', '/etc', '/dev', '/tmp', '/usr/adm', '/usr', '/'):
        parent = path.rsplit('/', 1)[0] or '/'
        children = [(child.rsplit('/', 1)[1], ino) for child, ino in entries.items()
                    if child != '/' and (child.rsplit('/', 1)[0] or '/') == path]
        image.directory(entries[path], entries[parent], sorted(children))
    return image.finish()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blocks', type=int, default=1024)
    parser.add_argument('--programs', type=Path, default=Path('build'))
    parser.add_argument('--config', type=Path, default=Path('rootfs'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        data = build(args.blocks, args.programs, args.config)
        args.output.write_bytes(data)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f'Built {args.output}: {len(data)//512} V7 blocks, init, shell and 7 utilities')

if __name__ == '__main__':
    main()

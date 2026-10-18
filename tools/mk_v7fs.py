"""Build PDP-11-format V7 filesystems for native roots and regression fixtures."""
import argparse
import mmap
import os
import tempfile
from pathlib import Path
import struct

BSIZE = 512
MAX_BLOCKS = 1 << 24
MAX_INODE_BLOCKS = 65535 // 8
MAX_FILE_BLOCKS = 10 + 128 + 128**2 + 128**3
DIR, REG, CHR, BLK = 0o40000, 0o100000, 0o20000, 0o60000

def short(value):
    return struct.pack('<H', value)

def long(value):
    return short(value >> 16) + short(value & 0xffff)

def address(value):
    if not 0 <= value <= 0xffffff:
        raise ValueError('inode address exceeds 24 bits')
    return bytes((value >> 16, value & 255, (value >> 8) & 255))

class Image:
    def __init__(self, blocks, inode_blocks=8):
        if type(blocks) is not int or not 256 <= blocks <= MAX_BLOCKS:
            raise ValueError('volume must have 256..16777216 blocks')
        if type(inode_blocks) is not int or not 1 <= inode_blocks <= MAX_INODE_BLOCKS or inode_blocks+2 >= blocks:
            raise ValueError('invalid inode area')
        self.blocks = blocks
        self.isize = 2+inode_blocks
        self.inodes = inode_blocks*8
        if blocks*BSIZE <= 32*1024*1024:
            self.bytes = bytearray(blocks*BSIZE)
        else:
            # Sparse temporary backing avoids reserving the whole volume in RAM.
            with tempfile.TemporaryFile() as backing:
                backing.truncate(blocks*BSIZE)
                self.bytes = mmap.mmap(backing.fileno(), blocks*BSIZE)
        self.next_block = self.isize
        self.allocated_inodes = {1}

    def allocate(self):
        if self.next_block == self.blocks:
            raise ValueError('filesystem fixture does not fit')
        block = self.next_block
        self.next_block += 1
        return block

    def sector(self, block):
        if type(block) is not int or not 0 <= block < self.blocks:
            raise ValueError('sector outside volume')
        return memoryview(self.bytes)[block * BSIZE:(block + 1) * BSIZE]

    def pointer(self, roots, logical):
        if type(logical) is not int or not 0 <= logical < MAX_FILE_BLOCKS:
            raise ValueError('file too large')
        if logical < 10:
            slot, indexes = logical, []
        else:
            remaining, span, depth = logical - 10, 128, 1
            while remaining >= span:
                remaining -= span
                span *= 128
                depth += 1
            if depth > 3:
                raise ValueError('file too large')
            slot, indexes = 9 + depth, []
            for _ in range(depth):
                span //= 128
                indexes.append(remaining // span)
                remaining %= span
        if not roots[slot]:
            roots[slot] = self.allocate()
        block = roots[slot]
        for index in indexes:
            sector = self.sector(block)
            offset = index * 4
            raw = bytes(sector[offset:offset + 4])
            next_block = (int.from_bytes(raw[:2], 'little') << 16) | int.from_bytes(raw[2:], 'little')
            if not next_block:
                next_block = self.allocate()
                sector[offset:offset + 4] = long(next_block)
            block = next_block
        return block

    def inode(self, number, mode, data=b'', sparse=None, device=0, nlink=1, uid=0, gid=0):
        if type(number) is not int or not 1 <= number <= self.inodes:
            raise ValueError('inode outside inode area')
        if len(data)>MAX_FILE_BLOCKS*BSIZE:
            raise ValueError('file too large')
        if sparse is not None and (type(sparse[0]) is not int or not 0 <= sparse[0] < MAX_FILE_BLOCKS or not 1<=len(sparse[1])<=BSIZE):
            raise ValueError('invalid sparse extent')
        if any(type(n) is not int or not 0<=n<=65535 for n in (mode,nlink,uid,gid)):
            raise ValueError('invalid inode metadata')
        if mode & 0o170000 in (CHR,BLK) and not 0<=device<=65535:
            raise ValueError('invalid device number')
        if nlink>32767:
            raise ValueError('too many inode links')
        self.allocated_inodes.add(number)
        roots = [0] * 13
        size = len(data)
        if sparse is not None:
            logical, payload = sparse
            block = self.pointer(roots, logical)
            self.sector(block)[:len(payload)] = payload
            size = logical * BSIZE + len(payload)
        elif mode & 0o170000 in (CHR, BLK):
            roots[0] = device
        else:
            for logical, offset in enumerate(range(0, size, BSIZE)):
                block = self.pointer(roots, logical)
                payload = data[offset:offset + BSIZE]
                self.sector(block)[:len(payload)] = payload
        record = (short(mode) + short(nlink) + short(uid) + short(gid) + long(size)
                  + b''.join(address(x) for x in roots) + b'\0' + long(0) * 3)
        assert len(record) == 64
        offset = 2 * BSIZE + (number - 1) * 64
        self.bytes[offset:offset + 64] = record

    def directory(self, number, parent, entries, mode=0o755, uid=0, gid=0):
        data = bytearray()
        names=set()
        for name, ino in [('.', number), ('..', parent)] + entries:
            encoded = name.encode('ascii')
            if not 1 <= len(encoded) <= 14 or '/' in name or '\0' in name or name in names:
                raise ValueError('directory component must fit 14 bytes')
            names.add(name)
            if not 1<=ino<=self.inodes:
                raise ValueError('directory inode outside inode area')
            data += short(ino) + encoded.ljust(14, b'\0')
        subdirectories = sum(
            (struct.unpack_from('<H', self.bytes, 2 * BSIZE + (ino - 1) * 64)[0] & 0o170000) == DIR
            for _, ino in entries)
        self.inode(number, DIR | mode, data, nlink=2 + subdirectories, uid=uid, gid=gid)

    def finish(self):
        # V7 free(): stack of 50, chained through a freed data block.
        stack = [0]
        # Allocations occupy one prefix; no volume-sized set/sort is needed.
        free_count = self.blocks-self.next_block
        for block in range(self.blocks-1, self.next_block-1, -1):
            if len(stack) == 50:
                payload = short(50) + b''.join(long(x) for x in stack)
                self.sector(block)[:len(payload)] = payload
                stack = []
            stack.append(block)
        superblock = self.sector(1)
        superblock[0:2] = short(self.isize)
        superblock[2:6] = long(self.blocks)
        superblock[6:8] = short(len(stack))
        for i, block in enumerate(stack):
            superblock[8 + 4 * i:12 + 4 * i] = long(block)
        spare_inodes = [i for i in range(3, (self.isize-2)*8+1) if i not in self.allocated_inodes]
        superblock[208:210] = short(min(100,len(spare_inodes)))
        for i, ino in enumerate(spare_inodes[:100]):
            superblock[210 + 2 * i:212 + 2 * i] = short(ino)
        superblock[418:422] = long(free_count)
        superblock[422:424] = short(len(spare_inodes))
        superblock[424:426] = short(1)
        superblock[426:428] = short(1)
        superblock[428:434] = b'nexix\0'
        superblock[434:440] = b'ram\0\0\0'
        return self.bytes

def file_blocks(size):
    """Count data and indirect blocks using the V7 10/128/128²/128³ layout."""
    if type(size) is not int or not 0<=size<=MAX_FILE_BLOCKS*BSIZE:
        raise ValueError('file too large')
    blocks=(size+BSIZE-1)//BSIZE
    total=blocks
    left=max(0,blocks-10)
    for depth in (1,2,3):
        n=min(left,128**depth)
        if n:
            total+=sum((n+128**level-1)//128**level for level in range(1,depth+1))
        left-=n
    return total


def write_image(path, data):
    """Publish only a complete image; never truncate an existing output on failure."""
    path=Path(path)
    if path.is_symlink():
        raise ValueError('refusing symlink image output')
    name=None
    try:
        with tempfile.NamedTemporaryFile(prefix='.nexix-image-',dir=path.parent,delete=False) as out:
            name=out.name
            for offset in range(0,len(data),1024*1024):
                out.write(memoryview(data)[offset:offset+1024*1024])
            out.flush();os.fsync(out.fileno())
        os.replace(name,path);name=None
    finally:
        if name is not None:
            os.unlink(name)


def build(blocks, exec_file, libc_file):
    image = Image(blocks)
    image.inode(5, REG | 0o644, b'Hello from the Nexix V7 filesystem!\n')
    image.inode(6, REG | 0o644, bytes((i * 17 + 3) & 255 for i in range(150 * BSIZE + 37)))
    image.inode(7, REG | 0o644, sparse=(10 + 128 + 128 * 128, b'TRIPLE'))
    image.inode(8, REG | 0o644)  # scratch file for writes
    image.inode(9, REG | 0o644, b'fourteen bytes\n')
    image.inode(10, REG | 0o755, exec_file.read_bytes())
    image.inode(11, REG | 0o755, libc_file.read_bytes())
    image.inode(13, CHR | 0o600, device=0x100, nlink=2)
    image.inode(14, CHR | 0o600, device=0x200)
    image.directory(3, 2, [('exec-test', 10), ('libc-test', 11)])
    image.directory(4, 2, [])
    image.directory(12, 2, [('console', 13), ('ttyS0', 13), ('tty', 14)])
    image.directory(2, 2, [('bin', 3), ('tmp', 4), ('hello', 5), ('indirect', 6),
                           ('sparse', 7), ('scratch', 8), ('abcdefghijklmn', 9), ('dev', 12)])
    return image.finish()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blocks', type=int, default=1024)
    parser.add_argument('--exec', dest='exec_file', type=Path, required=True)
    parser.add_argument('--libc', dest='libc_file', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        data = build(args.blocks, args.exec_file, args.libc_file)
        write_image(args.output, data)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(f'Built {args.output}: {len(data) // BSIZE} V7 blocks')

if __name__ == '__main__':
    main()

"""Check the generated root's disk bytes and allocation ownership independently."""
from pathlib import Path
import struct
import sys
import unittest
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from mk_rootfs import build

IMAGE = Path('build/root.img').read_bytes()
def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]
def u32(data, offset):
    return (u16(data, offset) << 16) | u16(data, offset + 2)

def inode(number):
    offset = 1024 + (number - 1) * 64
    record = IMAGE[offset:offset + 64]
    roots = [(record[i] << 16) | record[i+1] | (record[i+2] << 8)
             for i in range(12, 51, 3)]
    return u16(record, 0), u16(record, 2), u32(record, 8), roots

def contents(number, owned=None):
    mode, _, size, roots = inode(number)
    if mode & 0o170000 in (0o20000, 0o60000):
        return b''
    blocks = []
    def visit(block, depth):
        if not block:
            return
        if owned is not None:
            assert block not in owned, f'duplicate ownership: block {block}'
            assert 10 <= block < len(IMAGE)//512
            owned.add(block)
        if not depth:
            blocks.append(IMAGE[block*512:(block+1)*512])
        else:
            sector = IMAGE[block*512:(block+1)*512]
            for offset in range(0, 512, 4):
                visit(u32(sector, offset), depth-1)
    for block in roots[:10]:
        visit(block, 0)
    for depth, block in enumerate(roots[10:], 1):
        visit(block, depth)
    return b''.join(blocks)[:size]

def directory(number):
    data = contents(number)
    return {data[i+2:i+16].rstrip(b'\0').decode('ascii'): u16(data, i)
            for i in range(0, len(data), 16)}

def lookup(path):
    number = 2
    for name in path.strip('/').split('/'):
        if name:
            number = directory(number)[name]
    return number

class RootImage(unittest.TestCase):
    def test_programs_and_configuration(self):
        for name in ('init','sh','echo','cat','ls','pwd','cp','ln','rm','date','touch'):
            path = ('/etc/' if name == 'init' else '/bin/') + name
            number = lookup(path)
            self.assertEqual(inode(number)[0], 0o100755)
            self.assertEqual(contents(number), Path(f'build/v7-{name}.elf').read_bytes())
        for name in ('rc','ttys','passwd'):
            self.assertEqual(contents(lookup('/etc/'+name)), Path('rootfs/etc/'+name).read_bytes())
        self.assertEqual(contents(lookup('/.profile')), Path('rootfs/profile').read_bytes())
        for path in ('/etc/utmp','/usr/adm/wtmp'):
            self.assertEqual(contents(lookup(path)), b'')
        console = lookup('/dev/console')
        self.assertEqual(console, lookup('/dev/ttyS0'))
        self.assertEqual(inode(console)[:2], (0o20600, 2))
        self.assertEqual(inode(console)[3][0], 0x100)
        self.assertEqual(inode(lookup('/dev/tty'))[3][0], 0x200)
        self.assertEqual(inode(lookup('/dev/tty'))[0],0o20666)
        self.assertEqual(inode(lookup('/dev/null'))[0], 0o20666)
        self.assertEqual(inode(lookup('/dev/null'))[3][0], 0x300)

    def test_memory_devices(self):
        for name,dev in (('mem',0x600),('kmem',0x601)):
            self.assertEqual(inode(lookup('/dev/'+name))[0],0o20600)
            self.assertEqual(inode(lookup('/dev/'+name))[3][0],dev)
            self.assertEqual(contents(lookup('/dev/'+name)),b'')

    def test_controller_nodes(self):
        for kind,name in enumerate(('hp','rf','rk','rl','rp')):
            for raw in (False,True):
                record=inode(lookup('/dev/'+('r' if raw else '')+name+'0'))
                self.assertEqual(record[0],(0o20000 if raw else 0o60000)|0o600)
                self.assertEqual(record[3][0],((12 if raw else 7)+kind)*256)

    def test_tape_devices(self):
        for path,mode,dev in (('/dev/tapectl',0o20600,0x1500),('/dev/ht0',0o60600,0x1100),
                              ('/dev/rht0',0o20600,0x1300),('/dev/tm0',0o60600,0x1200),('/dev/rtm0',0o20600,0x1400)):
            record=inode(lookup(path))
            self.assertEqual(record[0],mode)
            self.assertEqual(record[3][0],dev)

    def test_dectape_devices(self):
        for unit in range(8):
            record=inode(lookup('/dev/tap'+str(unit)))
            self.assertEqual(record[0],0o60600)
            self.assertEqual(record[3][0],0x1600+unit)

    def test_storage_devices(self):
        self.assertEqual(inode(lookup('/dev/root'))[0], 0o60600)
        self.assertEqual(inode(lookup('/dev/root'))[3][0], 0)
        self.assertEqual(inode(lookup('/dev/rroot'))[0], 0o20600)
        self.assertEqual(inode(lookup('/dev/rroot'))[3][0], 0x400)

    def test_directory_links_and_space(self):
        directories = ('/','/bin','/etc','/dev','/tmp','/usr','/usr/adm')
        for path in directories:
            number = lookup(path)
            entries = directory(number)
            self.assertEqual(entries['.'], number)
            parent = path.rsplit('/',1)[0] or '/'
            self.assertEqual(entries['..'], lookup(parent))
            children = sum(inode(child)[0] & 0o170000 == 0o40000
                           for name, child in entries.items() if name not in ('.','..'))
            self.assertEqual(inode(number)[:2], (0o40755, 2+children))
        isize=u16(IMAGE,512)
        owned = set(range(isize))
        allocated = set()
        for number in range(2,(isize-2)*8+1):
            if inode(number)[0]:
                allocated.add(number)
                contents(number, owned)
        superblock = IMAGE[512:1024]
        stack = [u32(superblock, 8+i*4) for i in range(u16(superblock,6))]
        free = set()
        while stack:
            block = stack.pop()
            if not block:
                self.assertFalse(stack)
                break
            self.assertNotIn(block, owned | free)
            self.assertTrue(10 <= block < len(IMAGE)//512)
            free.add(block)
            if not stack:
                sector = IMAGE[block*512:(block+1)*512]
                count = u16(sector, 0)
                self.assertTrue(1 <= count <= 50)
                stack = [u32(sector, 2+i*4) for i in range(count)]
        self.assertEqual(owned | free, set(range(len(IMAGE)//512)))
        self.assertEqual(len(free), u32(superblock,418))
        spare = {i for i in range(3,(isize-2)*8+1) if i not in allocated}
        self.assertEqual(len(spare), u16(superblock,422))
        cached = {u16(superblock,210+i*2) for i in range(u16(superblock,208))}
        self.assertEqual(cached, spare)
        self.assertGreater(len(free), 100)

    def test_reproducible_build(self):
        self.assertEqual(build(len(IMAGE)//512, Path('build'), Path('rootfs')), IMAGE)

    def test_rejects_non_native_program(self):
        with tempfile.TemporaryDirectory() as temporary:
            programs = Path(temporary)
            bad = bytearray(Path('build/v7-init.elf').read_bytes())
            bad[18:20] = b'\x03\x00'  # ELF32 x86 machine, not x86_64.
            (programs / 'v7-init.elf').write_bytes(bad)
            with self.assertRaisesRegex(ValueError, 'expected native'):
                build(1024, programs, Path('rootfs'))

    def test_rejects_too_small_volume(self):
        with self.assertRaisesRegex(ValueError, 'does not fit'):
            build(256, Path('build'), Path('rootfs'))

if __name__ == '__main__':
    unittest.main()

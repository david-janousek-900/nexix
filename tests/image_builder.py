"""V7 image sizing, disk ownership, format limits and publication failures."""
import json
import mmap
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from install_manifest import MANIFEST
from mk_rootfs import build
from mk_v7fs import Image, DIR, REG, BSIZE, MAX_BLOCKS, MAX_INODE_BLOCKS, MAX_FILE_BLOCKS, file_blocks, write_image
from mk_large_image_fixture import fixture


def u16(data,offset):return struct.unpack_from('<H',data,offset)[0]
def u32(data,offset):return (u16(data,offset)<<16)|u16(data,offset+2)

def audit(image):
    """Independent disk decoder: every inode/data/pointer/free block accounted for."""
    blocks=len(image)//512;isize=u16(image,512);owned=set(range(isize));allocated={1}
    def tree(block,depth,out):
        if not block:return
        assert isize<=block<blocks and block not in owned
        owned.add(block)
        if not depth:out.append(block);return
        for offset in range(block*512,(block+1)*512,4):tree(u32(image,offset),depth-1,out)
    for ino in range(1,(isize-2)*8+1):
        start=1024+(ino-1)*64;mode=u16(image,start)
        if not mode:continue
        allocated.add(ino)
        if mode&0o170000 in (0o20000,0o60000):continue
        addresses=[(image[start+i]<<16)|image[start+i+1]|(image[start+i+2]<<8) for i in range(12,51,3)]
        data=[]
        for block in addresses[:10]:tree(block,0,data)
        for depth,block in enumerate(addresses[10:],1):tree(block,depth,data)
        assert len(data)*512>=u32(image,start+8)
    free=set();stack=[u32(image,520+4*i) for i in range(u16(image,518))]
    while stack:
        block=stack.pop()
        if not block:assert not stack;break
        assert isize<=block<blocks and block not in owned and block not in free
        free.add(block)
        if not stack:
            count=u16(image,block*512);assert 1<=count<=50
            stack=[u32(image,block*512+2+4*i) for i in range(count)]
    assert len(owned)+len(free)==blocks and len(free)==u32(image,930)
    spare={i for i in range(3,(isize-2)*8+1) if i not in allocated}
    cached=[u16(image,722+2*i) for i in range(u16(image,720))]
    assert len(spare)==u16(image,934) and len(cached)==min(len(spare),100)
    assert len(set(cached))==len(cached) and set(cached)<=spare
    return len(free),len(spare)


class Builder(unittest.TestCase):
    def test_automatic_inode_growth_and_payload_sizing(self):
        with tempfile.TemporaryDirectory() as temp:
            plan=json.loads(MANIFEST.read_text())
            for i in range(1500):plan['entries'].append(dict(path=f'/tmp/f{i:04d}',type='file',format='empty',mode='0600',uid=123,gid=456,component='distribution.config'))
            path=Path(temp)/'install.json';path.write_text(json.dumps(plan))
            image=build(None,Path('build'),Path('rootfs'),path,spare_inodes=240,spare_blocks=256)
            self.assertGreater(u16(image,512)-2,125)
            blocks,inodes=audit(image);self.assertEqual(blocks,256);self.assertGreaterEqual(inodes,240)
            with self.assertRaisesRegex(ValueError,'inode area'):build(4096,Path('build'),Path('rootfs'),path,inode_blocks=12)
            with self.assertRaisesRegex(ValueError,'does not fit'):build(256,Path('build'),Path('rootfs'),path)

    def test_large_native_filesystem(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'root.img';fixture(path)
            with path.open('rb') as file:
                with mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as image:
                    self.assertEqual(u32(image,514),131072);self.assertEqual(u16(image,512),514);audit(image)
            subprocess.run(['build/image-scale-test',str(path)],check=True)

    def test_indirect_boundaries_and_counts(self):
        for n in (0,1,10,11,138,139,16650,MAX_FILE_BLOCKS):
            image=Image(512)
            if not n:self.assertEqual(file_blocks(0),0);continue
            image.inode(3,REG|0o600,sparse=(n-1,b'x'))
            overhead=file_blocks(n*512)-n
            # Sparse last blocks need only the pointer path, not every dense subtree.
            self.assertGreaterEqual(overhead,image.next_block-image.isize-1)
            image.directory(2,2,[('sparse',3)]);raw=image.finish()
            self.assertEqual(u32(raw,1024+2*64+8),(n-1)*512+1)
        image=Image(1024);data=b'x'*(139*512);image.inode(3,REG|0o600,data)
        self.assertEqual(image.next_block-image.isize,file_blocks(len(data)))
        image.directory(2,2,[('dense',3)]);audit(image.finish())

    def test_native_format_bounds_without_large_allocation(self):
        with patch('mk_v7fs.mmap.mmap',return_value=bytearray(8)) as mapping:
            image=Image(MAX_BLOCKS,MAX_INODE_BLOCKS)
            self.assertEqual(mapping.call_args.args[1],8*1024**3)
            self.assertEqual(image.inodes,65528)
        for blocks,inodes in ((MAX_BLOCKS+1,8),(255,8),(1024,MAX_INODE_BLOCKS+1),(256,254),(True,8)):
            with self.assertRaises(ValueError):Image(blocks,inodes)
        image=Image(10000,MAX_INODE_BLOCKS);image.inode(65528,REG|0o600)
        for number in (0,65529,-1):
            before=image.bytes[:]
            with self.assertRaises(ValueError):image.inode(number,REG|0o600)
            self.assertEqual(before,image.bytes)
        with self.assertRaises(ValueError):image.inode(3,REG,sparse=(MAX_FILE_BLOCKS,b'x'))
        with self.assertRaises(ValueError):file_blocks(MAX_FILE_BLOCKS*512+1)

    def test_cli_failure_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'disk';path.write_bytes(b'keep')
            for flags in (['--inodes','1'],['--blocks','256'],['--blocks',str(MAX_BLOCKS+1)]):
                result=subprocess.run([sys.executable,'tools/mk_rootfs.py','--output',str(path),*flags],capture_output=True)
                self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),b'keep')
            from install_manifest import load
            requested=8*((len(load())+31)//8)
            subprocess.run([sys.executable,'tools/mk_rootfs.py','--blocks','auto','--inodes',str(requested),'--output',str(path)],check=True,capture_output=True)
            raw=path.read_bytes();self.assertEqual(u16(raw,512),2+requested//8);audit(raw)

    def test_failed_atomic_publication_and_symlinks(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'disk';path.write_bytes(b'keep')
            with patch('mk_v7fs.os.replace',side_effect=OSError('write fault')):
                with self.assertRaises(OSError):write_image(path,b'new')
            self.assertEqual(path.read_bytes(),b'keep');self.assertEqual(list(Path(temp).iterdir()),[path])
            link=Path(temp)/'link';link.symlink_to(path)
            with self.assertRaises(ValueError):write_image(link,b'new')
            self.assertEqual(path.read_bytes(),b'keep')


if __name__=='__main__':unittest.main()

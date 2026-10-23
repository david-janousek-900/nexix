"""Offline upgrade preservation, validation and publication-failure contracts."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from mk_v7fs import Image,REG,DIR,CHR,BLK,long,write_image
from install_manifest import load_retired,stage_tree
from upgrade_disk import Volume,upgrade


class Upgrade(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.disk=self.root/'disk.img';self.backup=self.root/'backup.img'
        image=Image(1024,8)
        image.inode(7,REG|0o755,b'old program',nlink=3)   # also the retired /etc/oldtool
        image.inode(8,REG|0o600,b'root:custom:0:0:Owner:/:/bin/sh\n')
        image.inode(9,REG|0o644,b'TZ=CET-1\nexport TZ\n')
        image.inode(10,REG|0o640,sparse=(10+128+128*128,b'SPARSE'),nlink=3,uid=100,gid=100)
        image.inode(12,CHR|0o620,device=256,nlink=2,uid=100,gid=100)
        image.inode(13,BLK|0o600,device=4096)
        image.inode(14,REG|0o755,b'/etc/oldtool em0 10.0.2.15/24 up\n')   # an administrator's /etc/netstart
        image.directory(15,4,[],mode=0o755)                                   # a retired path reused as a directory
        image.inode(17,REG|0o644,b'PATH=/bin\nexport PATH\n')              # root's old profile
        image.directory(16,2,[('.profile',17)],mode=0o700)
        image.directory(6,5,[('keep',10),('alias',10),('toolcopy',7),('rawname',10)],mode=0o700,uid=100,gid=100)
        offset=1024+(6-1)*64+12
        block=(image.bytes[offset]<<16)|image.bytes[offset+1]|(image.bytes[offset+2]<<8)
        image.bytes[block*512+5*16+2:block*512+6*16]=b'\xffname'.ljust(14,b'\0')
        image.directory(5,2,[('alice',6)])
        image.directory(3,2,[('tool',7)])
        image.directory(4,2,[('passwd',8),('netstart',14),('oldtool',7),('olddir',15)])
        image.directory(11,2,[('console',12),('ttyS0',12),('disk',13)])
        image.directory(2,2,[('bin',3),('etc',4),('home',5),('dev',11),('.profile',9),('root',16)])
        for ino in image.allocated_inodes-{1}:
            image.bytes[1024+(ino-1)*64+52:1024+ino*64]=long(0x12345678)+long(123)+long(456)
        self.original=bytes(image.finish());write_image(self.disk,self.original)
        self.programs=self.root/'programs';self.config=self.root/'config';self.stage=self.root/'stage'
        self.programs.mkdir();self.config.mkdir()
        (self.programs/'tool').write_bytes(b'new program\n'*100)
        (self.config/'passwd').write_bytes(b'new default accounts\n')
        (self.config/'profile').write_bytes(b'TZ=GMT0\n')
        (self.config/'newconf').write_bytes(b'new configuration\n')
        records=[]
        def add(path,kind,mode='0755',source=None,**kw):
            e=dict(path=path,type=kind,mode=mode,uid=0,gid=0,component='distribution.config',**kw)
            if source:e.update(source=source,format='data')
            records.append(e)
        for name in ['/','/bin','/etc','/home','/home/alice','/dev','/root']:
            add(name,'directory','0700' if name=='/root' else '0755')
        add('/bin/tool','file','4755',dict(root='programs',path='tool'))
        records.append(dict(path='/bin/link',type='link',target='/bin/tool',component='distribution.config'))
        for name,source in [('/etc/passwd','passwd'),('/.profile','profile'),('/root/.profile','profile'),('/etc/newconf','newconf')]:
            add(name,'file','0644',dict(root='config',path=source))
        add('/dev/console','character','0600',device=256)
        records.append(dict(path='/dev/ttyS0',type='link',target='/dev/console',component='distribution.devices'))
        add('/dev/new','character','0600',device=257)
        retired=[dict(path='/etc/oldtool',replacement='/bin/tool',task=432),dict(path='/etc/olddir',replacement='/bin/tool',task=432),
                 dict(path='/etc/absent',replacement='/bin/tool',task=432)]
        self.manifest=self.root/'manifest.json';self.manifest.write_text(json.dumps(dict(schema_version=1,version='0.2.0',inventory='docs/v7-userland-manifest.json',retired=retired,entries=records)))
        stage_tree(self.stage,self.manifest,self.programs,self.config)

    def run_upgrade(self,apply=True):
        return upgrade(self.disk,self.stage,self.manifest,self.backup,apply,1700000000)

    def test_preview_and_preservation(self):
        report=self.run_upgrade(False)
        self.assertFalse(report['applied']);self.assertEqual(self.disk.read_bytes(),self.original);self.assertFalse(self.backup.exists())
        self.assertIn('/etc/passwd',report['preserved_configuration'])
        self.assertTrue(self.run_upgrade()['applied'])
        self.assertEqual(self.backup.read_bytes(),self.original)
        before=Volume(self.original);after=Volume(self.disk.read_bytes())
        for name in ['/etc/passwd','/etc/netstart','/.profile','/home/alice/keep','/home/alice/alias','/home/alice/toolcopy','/home/alice/\udcffname','/dev/console','/dev/disk']:
            a,b=before.paths[name],after.paths[name]
            self.assertEqual((a.ino,a.mode,a.uid,a.gid,a.device,a.times),(b.ino,b.mode,b.uid,b.gid,b.device,b.times),name)
            if a.kind==REG:self.assertEqual(before.hash(a),after.hash(b),name)
        self.assertIs(after.paths['/home/alice/keep'],after.paths['/home/alice/alias'])
        self.assertIs(after.paths['/home/alice/keep'],after.paths['/home/alice/\udcffname'])
        for name in ['/home','/home/alice']:
            self.assertEqual(after.paths[name].times,before.paths[name].times)
        self.assertIs(after.paths['/dev/console'],after.paths['/dev/ttyS0'])
        self.assertIs(after.paths['/bin/tool'],after.paths['/bin/link'])
        self.assertIsNot(after.paths['/bin/tool'],after.paths['/home/alice/toolcopy'])
        self.assertEqual(after.contents(after.paths['/bin/tool']),(self.programs/'tool').read_bytes())
        self.assertEqual(after.paths['/bin/tool'].mode,REG|0o4755)
        self.assertEqual(after.paths['/bin/tool'].times,(1700000000,)*3)
        self.assertEqual(after.paths['/root'].mode&0o777,0o700)
        self.assertEqual(after.contents(after.paths['/etc/newconf']),b'new configuration\n')
        data=self.disk.read_bytes();self.assertFalse(self.run_upgrade()['applied']);self.assertEqual(self.disk.read_bytes(),data)

    def test_retired_paths(self):
        report=self.run_upgrade(False)
        self.assertIn({'action':'remove','path':'/etc/oldtool'},report['changes'])
        self.assertEqual(report['retired_kept'],['/etc/olddir'])
        self.assertEqual(report['warnings'],['/etc/netstart still names retired /etc/oldtool; use /bin/tool',
                                             '/root/.profile PATH lacks /sbin and /usr/sbin, where system commands now live'])
        self.assertEqual(self.disk.read_bytes(),self.original)
        self.assertTrue(self.run_upgrade()['applied'])
        before=Volume(self.original);after=Volume(self.disk.read_bytes())
        self.assertNotIn('/etc/oldtool',after.paths)                      # removed, with no link left behind
        self.assertEqual(after.paths['/etc/olddir'].kind,DIR)              # not a regular file: kept
        old=after.paths['/home/alice/toolcopy']                            # the other link keeps the old bytes
        self.assertEqual(after.contents(old),b'old program')
        self.assertEqual(old.ino,before.paths['/home/alice/toolcopy'].ino)
        for name in ['/etc/netstart','/root/.profile']:                     # configuration is never rewritten
            self.assertEqual(after.hash(after.paths[name]),before.hash(before.paths[name]))
        report=self.run_upgrade(False)
        self.assertFalse(any(c['action']=='remove' for c in report['changes']))   # nothing left to retire

    def test_retired_list_is_validated(self):
        data=json.loads(self.manifest.read_text())
        for bad in [dict(path='/etc/x',replacement='/missing',task=432),dict(path='/bin/tool',replacement='/bin/tool',task=432),
                    dict(path='/etc/x',replacement='/bin/tool'),dict(path='etc/x',replacement='/bin/tool',task=432)]:
            data['retired']=[bad];self.manifest.write_text(json.dumps(data))
            with self.subTest(bad=bad),self.assertRaisesRegex(ValueError,'retired|path'):load_retired(self.manifest)

    def test_existing_backup_is_never_overwritten(self):
        self.backup.write_bytes(b'keep backup')
        with self.assertRaises(ValueError):self.run_upgrade()
        self.assertEqual(self.backup.read_bytes(),b'keep backup');self.assertEqual(self.disk.read_bytes(),self.original)

    def test_backup_is_mandatory(self):
        with self.assertRaises(ValueError):upgrade(self.disk,self.stage,self.manifest,apply=True)
        self.assertEqual(self.disk.read_bytes(),self.original)

    def test_candidate_failure_preserves_original_and_complete_backup(self):
        with patch('upgrade_disk.build_upgrade',side_effect=OSError('simulated full disk')):
            with self.assertRaises(OSError):self.run_upgrade()
        self.assertEqual(self.disk.read_bytes(),self.original);self.assertEqual(self.backup.read_bytes(),self.original)
        self.assertFalse(list(self.root.glob('.nexix-*')))

    def test_publication_failure_preserves_original_and_backup(self):
        with patch('upgrade_disk.os.replace',side_effect=OSError('simulated rename failure')):
            with self.assertRaises(OSError):self.run_upgrade()
        self.assertEqual(self.disk.read_bytes(),self.original);self.assertEqual(self.backup.read_bytes(),self.original)
        self.assertFalse(list(self.root.glob('.nexix-*')))

    def test_symlink_and_hardlinked_images_are_rejected(self):
        link=self.root/'link';link.symlink_to(self.disk)
        with self.assertRaises(OSError):upgrade(link,self.stage,self.manifest,self.backup,True)
        import os
        link.unlink();os.link(self.disk,link)
        with self.assertRaises(ValueError):self.run_upgrade()
        self.assertEqual(self.disk.read_bytes(),self.original)

    def test_active_image_is_rejected(self):
        code="import fcntl,sys; f=open(sys.argv[1],'r+b'); fcntl.lockf(f,fcntl.LOCK_EX); print('ready',flush=True); sys.stdin.readline()"
        child=subprocess.Popen([sys.executable,'-c',code,str(self.disk)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
        try:
            self.assertEqual(child.stdout.readline(),'ready\n')
            with self.assertRaises(BlockingIOError):self.run_upgrade()
        finally:child.communicate('\n',timeout=5)
        self.assertFalse(self.backup.exists());self.assertEqual(self.disk.read_bytes(),self.original)

    def test_corruption_is_rejected_before_backup(self):
        for offset,payload in [(512,b'\0\0'),(1024+(10-1)*64+12,b'\0\1\0'),(520,long(2))]:
            broken=bytearray(self.original);broken[offset:offset+len(payload)]=payload;self.disk.write_bytes(broken)
            with self.assertRaises(ValueError):self.run_upgrade()
            self.assertEqual(self.disk.read_bytes(),broken);self.assertFalse(self.backup.exists())

    def test_modified_stage_is_rejected(self):
        (self.stage/'root/bin/tool').write_bytes(b'tampered')
        with self.assertRaises(ValueError):self.run_upgrade()
        self.assertEqual(self.disk.read_bytes(),self.original);self.assertFalse(self.backup.exists())

    def test_type_conflict_is_rejected(self):
        data=bytearray(self.original);offset=1024+(7-1)*64
        # A valid directory at the incoming executable path is an upgrade conflict.
        source=Volume(self.original)
        records,payloads=__import__('install_manifest').read_stage(self.stage,self.manifest)
        from upgrade_disk import plan
        source.paths['/bin/tool']=source.paths['/home']
        with self.assertRaisesRegex(ValueError,'type conflict'):plan(source,records,payloads,1700000000)


if __name__=='__main__':unittest.main()

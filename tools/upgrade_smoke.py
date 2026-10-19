"""Upgrade disposable customized production images, then cold-boot them twice."""
from pathlib import Path
import json
import sys
import tempfile
from guest_acceptance import Guest
from multiuser_acceptance import login
from mk_v7fs import REG,write_image
from upgrade_disk import Volume,Node,build_upgrade,upgrade,digest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with tempfile.TemporaryDirectory(prefix='nx-upgrade-') as temp:
        disk=Path(temp)/'root.img';backup=Path(temp)/'before.img'
        source=Volume(Path('build/distribution.img').read_bytes());paths=dict(source.paths)
        def replace(path,payload,mode=None):
            old=paths.get(path)
            node=Node(old.ino if old else 0,mode if mode is not None else old.mode,
                      old.uid if old else 0,old.gid if old else 0,(123,456,789),len(payload),payload=payload)
            paths[path]=node;return node
        replace('/bin/echo',source.contents(source.paths['/bin/sync']))
        passwd=source.contents(source.paths['/etc/passwd']).replace(b':Root:',b':Preserved owner:')
        replace('/etc/passwd',passwd)
        profile=b'PATH=/bin\nHOME=/root\nTZ=CET-1\nexport PATH HOME TZ\n'
        replace('/root/.profile',profile)
        saved=replace('/root/keep',b'PRESERVED_DATA\n',REG|0o640);paths['/root/alias']=saved
        data=build_upgrade(source,paths,1700000000);write_image(disk,data)
        if hasattr(data,'close'):data.close()
        before=disk.read_bytes();old=Volume(before)
        assert old.hash(old.paths['/bin/echo'])!=source.hash(source.paths['/bin/echo'])
        report=upgrade(disk,Path('build/stage'),backup_path=backup,apply=True,epoch=1700000001)
        assert report['applied'] and digest(backup.read_bytes())==digest(before)
        after=Volume(disk.read_bytes())
        for name in ['/etc/passwd','/root/.profile','/root/keep','/root/alias']:
            a,b=old.paths[name],after.paths[name]
            assert (a.ino,a.mode,a.uid,a.gid,a.times)==(b.ino,b.mode,b.uid,b.gid,b.times),name
            assert old.hash(a)==after.hash(b),name
        assert after.paths['/root/keep'] is after.paths['/root/alias']
        assert after.hash(after.paths['/bin/echo'])==source.hash(source.paths['/bin/echo'])
        for boot in range(2):
            guest=Guest(qemu,iso,disk,ram)
            try:
                login(guest,'root','nexix')
                guest.command('echo UPGRADED_OK',stdout='UPGRADED_OK\n')
                guest.command('echo $TZ',stdout='CET-1\n')
                guest.command('cat /root/keep /root/alias',stdout='PRESERVED_DATA\n'*2)
                if not boot:guest.command('echo PERSISTENT_AFTER_UPGRADE > /root/newfile')
                guest.command('cat /root/newfile',stdout='PERSISTENT_AFTER_UPGRADE\n')
                guest.command('sync')
            finally:
                Path(f'build/upgrade-{ram}-boot{boot}.log').write_bytes(guest.buffers[0]);guest.close()
        Path(f'build/upgrade-{ram}.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {ram} offline upgrade, exact backup, binary replacement, accounts/TZ/data/link/timestamp preservation and two cold boots')


if __name__=='__main__':main()

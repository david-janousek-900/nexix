"""Exercise full-only installation changes without editing tracked manifests."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from install_manifest import MANIFEST


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fixture(base):
    base=Path(base);config=base/'config';shutil.copytree('rootfs',config)
    plan=json.loads(MANIFEST.read_text())
    common=dict(type='file',format='data',mode='0644',uid=0,gid=0,component='distribution.config')
    for name in ('full-data','full-only'):
        plan['entries'].append(dict(path='/usr/'+name,source=dict(root='config',path=name),**common))
    plan['entries'].append(dict(path='/usr/echo',type='link',target='/bin/echo',component='cmd.echo'))
    manifest=base/'install.json';manifest.write_text(json.dumps(plan))
    stage=base/'stage';image=base/'distribution.img';disk=base/'working.img'
    args=[f'INSTALL_MANIFEST={manifest}',f'INSTALL_CONFIG={config}',f'INSTALL_STAGE={stage}',f'FULL_ROOT_IMAGE={image}']
    kernel=sha('build/nexix.elf');bootstrap=sha('build/root.img')
    def make(target):subprocess.run(['make','--no-print-directory',target,*args],check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    # Deliberately missing full-only inputs must not block the kernel build.
    make('all');assert not stage.exists()
    assert sha('build/nexix.elf')==kernel and sha('build/root.img')==bootstrap
    (config/'full-only').write_bytes(b'FULL_ONLY_DATA\n')
    (config/'etc/rc').write_bytes(b'# FULL_IDE_CONFIG\n')
    with (config/'full-data').open('wb') as f:f.truncate(8*1024*1024)
    make('rootfs');make('all')
    assert image.stat().st_size>8*1024*1024
    assert sha('build/nexix.elf')==kernel and sha('build/root.img')==bootstrap
    subprocess.run(['make','--no-print-directory','disk',*args,f'DISK_IMAGE={disk}'],check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    assert sha(image)==sha(disk)
    # Preserve an existing disk even after its contents differ from the seed.
    with disk.open('r+b') as f:f.seek(-1,2);f.write(b'X')
    saved=sha(disk)
    subprocess.run(['make','--no-print-directory','disk',*args,f'DISK_IMAGE={disk}'],check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    assert sha(disk)==saved
    return disk

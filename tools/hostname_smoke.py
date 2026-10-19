"""Persistent FQDN, real credential prompts and three cold boots per RAM size."""
from contextlib import contextmanager
from pathlib import Path
import sys
import tempfile
from guest_acceptance import Guest
from multiuser_acceptance import login
from mk_rootfs import build
from mk_v7fs import write_image
import json

qemu,iso,ram=sys.argv[1:4]
@contextmanager
def boot(disk,phase):
    guest=Guest(qemu,iso,disk,ram,2)
    try:yield guest
    finally:
        assert not any(b'#: not found' in data for data in guest.buffers),'profile comment was executed as a command'
        for n,data in enumerate(guest.buffers):Path(f'build/hostname-{phase}-{ram}.tty{n}.log').write_bytes(data)
        guest.close()

with tempfile.TemporaryDirectory(prefix='nx-hostname-') as temp:
    plan=json.loads(Path('docs/native-install.json').read_text())
    plan['entries'].append(dict(path='/tmp/host-api',type='file',mode='0755',uid=0,gid=0,component='distribution.config',source=dict(root='programs',path='hostname-api.elf'),format='data'))
    manifest=Path(temp)/'install.json';manifest.write_text(json.dumps(plan))
    data=build(None,Path('build'),Path('rootfs'),manifest,spare_blocks=8192)
    disk=Path(temp)/'root.img';write_image(disk,data)
    if hasattr(data,'close'):data.close()
    with boot(disk,'set') as guest:
        login(guest,'root','nexix',prompt='root@nexix.localdomain# ')
        login(guest,'alice','alice',1,prompt='alice@nexix.localdomain$ ')
        guest.command('hostname',stdout='nexix.localdomain\n')
        guest.command('hostname node.example.test',prompt='root@node.example.test# ')
        guest.command('hostname -f',stdout='node.example.test\n')
        guest.command('hostname -s',stdout='node\n')
        guest.command('hostname -d',stdout='example.test\n')
        guest.command("PS1='\\u@\\h\\$ '",prompt='root@node# ')
        guest.command("PS1='\\u@\\H\\$ '",prompt=r'root@node\.example\.test# ')
        guest.command('hostname -z',stdout='usage: hostname [-f|-s|-d] | hostname name | hostname -F file\n',status=2)

        guest.command('uname -n',stdout='node.example.test\n')
        guest.command('cat /etc/hostname',stdout='node.example.test\n')
        guest.command('/tmp/host-api node.example.test',stdout='HOSTNAME_API_OK\n')
        guest.command('echo LIVE',1,stdout='LIVE\n',prompt=r'alice@node\.example\.test\$ ')
        guest.command('/tmp/host-api node.example.test',1,stdout='HOSTNAME_API_OK\n',prompt=r'\$ ')
        guest.command('hostname denied.example',1,stdout='hostname: Not owner\n',status=1,prompt=r'\$ ')
        guest.command("hostname 'bad name'",stdout='hostname: invalid hostname\n',status=1)
        guest.command('echo runtime.example.test > /tmp/loadhost')
        guest.command('hostname -F /tmp/loadhost',prompt='root@runtime.example.test# ')
        guest.command('cat /etc/hostname',stdout='node.example.test\n')
        guest.command('hostname -F /etc/hostname',prompt='root@node.example.test# ')
        guest.command("echo 'bad name' > /tmp/loadhost")
        guest.command('hostname -F /tmp/loadhost',stdout='hostname: Invalid argument\n',status=1)
        guest.command('mv /etc/hostname /etc/hostname-old')
        guest.command('mkdir /etc/hostname')
        guest.command('hostname fail.example',stdout='hostname: /etc/hostname: Is a directory\n',status=1)
        guest.command('uname -n',stdout='node.example.test\n')
        guest.command('rmdir /etc/hostname; mv /etc/hostname-old /etc/hostname')
        guest.send('su alice\n');guest.expect(r'alice@node\.example\.test\$ ')
        guest.send('su\n');guest.expect('Password:');guest.send('nexix\n');guest.expect('root@node.example.test# ')
        guest.send('\x04');guest.expect(r'alice@node\.example\.test\$ ')
        guest.send('\x04');guest.expect('root@node.example.test# ')
        guest.command('sync')
    with boot(disk,'persisted') as guest:
        login(guest,'root','nexix',prompt='root@node.example.test# ')
        guest.command('hostname',stdout='node.example.test\n')
        guest.command('echo manual.domain.test > /etc/hostname')
        guest.command('hostname',stdout='node.example.test\n')
        # Existing home profiles need not contain PS1: /etc/profile supplies it.
        guest.command("echo 'PATH=/bin' > /root/.profile; echo 'export PATH' >> /root/.profile")
        guest.command('sync')
    with boot(disk,'edited') as guest:
        login(guest,'root','nexix',prompt='root@manual.domain.test# ')
        login(guest,'alice','alice',1,prompt='alice@manual.domain.test$ ')
        guest.command('hostname',stdout='manual.domain.test\n')
        guest.command('cat /etc/hostname',stdout='manual.domain.test\n')
print(f'PASS: {ram} hostname FQDN persistence, manual config boot, API/privileges/errors, live root/user prompts and su')

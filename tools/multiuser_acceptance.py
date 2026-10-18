"""Cold-boot the production seed, persist credentials/data and audit recovery."""
from contextlib import contextmanager
import hashlib
from pathlib import Path
import re
import struct
import sys
import tempfile
from guest_acceptance import Guest
from private_guest import private_guest
from install_manifest import ROOT
from seed_disk import seed_disk


def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''):digest.update(data)
    return digest.hexdigest()


def login(guest,name,password,line=0,prompt='# ',retry=False):
    guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line)
    if retry:
        guest.send('incorrect\n',line);guest.expect('Login incorrect\r\nlogin: ',line)
        guest.send(name+'\n',line);guest.expect('Password:',line)
    start=len(guest.buffers[line]);guest.send(password+'\n',line);guest.expect(re.escape(prompt),line)
    if password.encode() in guest.buffers[line][start:]:raise AssertionError('password echoed')


@contextmanager
def boot(qemu,iso,disk,ram,phase,rtc):
    guest=Guest(qemu,iso,disk,ram,2,rtc=rtc)
    try:yield guest
    finally:
        for n,out in enumerate(guest.buffers):(ROOT/'build'/f'multiuser-{phase}-{ram}.tty{n}.log').write_bytes(out)
        guest.diagnostics.seek(0);(ROOT/'build'/f'multiuser-{phase}-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
        guest.close()


def metadata(disk,path):
    """Independently decode the small direct directories used by these fixtures."""
    data=Path(disk).read_bytes()
    def inode(number):return data[1024+(number-1)*64:1024+number*64]
    def long(record,offset):return (int.from_bytes(record[offset:offset+2],'little')<<16)|int.from_bytes(record[offset+2:offset+4],'little')
    def contents(record):
        size=long(record,8)
        if size>512:raise AssertionError('fixture needs more than one direct block')
        block=(record[12]<<16)|(record[14]<<8)|record[13]
        return data[block*512:block*512+size]
    number=2
    for part in path.strip('/').split('/'):
        directory=contents(inode(number));found=None
        for offset in range(0,len(directory),16):
            n=int.from_bytes(directory[offset:offset+2],'little')
            if n and directory[offset+2:offset+16].split(b'\0')[0].decode()==part:found=n;break
        if found is None:raise AssertionError('missing persisted path: '+path)
        number=found
    record=inode(number);mode,links,uid,gid=struct.unpack_from('<HHHH',record)
    return mode,links,uid,gid,tuple(long(record,o) for o in (52,56,60)),contents(record)


def main():
    qemu,iso,ram=sys.argv[1:4];seed=ROOT/'build/distribution.img';working=ROOT/'build/nexix.img'
    original=sha(working) if working.is_file() else None
    with tempfile.TemporaryDirectory(prefix='nx-multiuser-release-') as temp:
        disk=Path(temp)/'root.img';assert seed_disk(seed,disk) and sha(seed)==sha(disk)
        with boot(qemu,iso,disk,ram,'first','2024-02-29T12:34:56') as guest:
            login(guest,'root','nexix',retry=True);login(guest,'alice','alice',1,'$ ')
            guest.command('tty',stdout='/dev/console\n');guest.command('tty',1,'/dev/ttyS1\n',prompt=re.escape('$ '))
            guest.command('echo ALICE_PERSIST > persist',1,prompt=re.escape('$ '))
            guest.command('echo denied 2>/tmp/denied >/etc/forbidden',1,stdout='/etc/forbidden: cannot create\n',status=1,prompt=re.escape('$ '))
            guest.command('mesg n',1,status=1,prompt=re.escape('$ '))
            guest.command('write alice',stdout='Permission denied\n',status=1)
            guest.command('mesg y',1,prompt=re.escape('$ '))
            wire='write alice; echo WRITE_STATUS_$?';guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'))
            guest.expect(re.escape('Message from root console...\r\n\x07\x07\x07'),1)
            guest.send('TWO_USER_MESSAGE\n');guest.expect('TWO_USER_MESSAGE\r\n',1)
            guest.send('\x04');guest.expect('EOF\r\n',1);guest.expect('WRITE_STATUS_0\r\n# ')
            guest.command('echo BOOT_BROADCAST > /tmp/broadcast')
            wire='/etc/wall /tmp/broadcast; echo WALL_STATUS_$?';guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'))
            message=re.escape('\x07\x07\x07Broadcast Message ...\r\n\r\nBOOT_BROADCAST\r\n')
            guest.expect(message);guest.expect('WALL_STATUS_0\r\n# ');guest.expect(message,1)
            guest.send('passwd; echo PASSWORD_STATUS_$?\n',1);guest.expect('Old password:',1);guest.send('alice\n',1)
            guest.expect('New password:',1);guest.send('Persist7\n',1);guest.expect('Retype new password:',1);guest.send('Persist7\n',1)
            guest.expect('PASSWORD_STATUS_1\r\n'+re.escape('$ '),1)
            guest.send('newgrp other\n',1);guest.expect('newgrp other\r\n'+re.escape('$ '),1)
            guest.command('echo GROUP_OWNER > group-owned',1,prompt=re.escape('$ '))
            guest.send('su alice; echo SU_STATUS_$?\n');guest.expect(re.escape('$ '))
            guest.command('echo SU_OWNER > /tmp/su-owned',prompt=re.escape('$ '));guest.send('\x04');guest.expect('SU_STATUS_0\r\n# ')
            guest.send('\x13echo FLOW_AFTER_LOGIN\n',1)
            guest.command('echo OTHER_TTY_ALIVE',stdout='OTHER_TTY_ALIVE\n')
            guest.send('\x11',1);guest.expect('FLOW_AFTER_LOGIN\r\n'+re.escape('$ '),1)
            guest.send('\x04',1);login(guest,'alice','Persist7',1,'$ ')
            guest.send('\n'*8,1)
            for _ in range(8):guest.expect(re.escape('$ '),1)
            guest.command('cat persist',1,'ALICE_PERSIST\n',prompt=re.escape('$ '))
        mode,links,uid,gid,times,payload=metadata(disk,'/home/alice/persist')
        assert (mode,links,uid,gid,payload)==(0o100664,1,100,100,b'ALICE_PERSIST\n') and all(t>=1709210096 for t in times)
        assert metadata(disk,'/home/alice/group-owned')[2:4]==(100,1)
        assert metadata(disk,'/tmp/su-owned')[2:4]==(100,100)
        saved=sha(disk);assert not seed_disk(seed,disk) and sha(disk)==saved
        with boot(qemu,iso,disk,ram,'reboot','2024-03-01T01:02:03') as guest:
            login(guest,'root','nexix');login(guest,'alice','Persist7',1,'$ ')
            guest.command('cat persist',1,'ALICE_PERSIST\n',prompt=re.escape('$ '))
            guest.command('cat group-owned',1,'GROUP_OWNER\n',prompt=re.escape('$ '))
            guest.command('cat /tmp/su-owned',stdout='SU_OWNER\n')
            guest.send('date -u\n');guest.expect(r'Fri Mar  1 01:02:[0-5][0-9] GMT 2024\r\n# ')
            guest.send('who\n');guest.expect(r'root {5}console Mar  1 01:[0-5][0-9]\r\nalice {4}ttyS1 {3}Mar  1 01:[0-5][0-9]\r\n# ')
        assert metadata(disk,'/home/alice/persist')[4][1]==times[1]
    # Separate private observer acceptance measures real production init/login,
    # without adding an observer to the production-seed cold boots above.
    with private_guest(qemu,iso,ram,'multiuser-resources',{'/bin/resources':'session-resources.elf'},2,single_user=False) as guest:
        login(guest,'root','nexix');login(guest,'bob','bob',1,'$ ')
        def resources():
            wire='/bin/resources; echo RESOURCE_STATUS_$?';guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'))
            before,match=guest.expect(r'RES ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\r\nRESOURCE_STATUS_0\r\n# ')
            if before:raise AssertionError('unexpected resource observer output')
            return tuple(int(match[i]) for i in range(1,6))
        resources();baseline=resources()
        for _ in range(4):
            guest.send('\x04',1);login(guest,'bob','bob',1,'$ ')
            if resources()!=baseline:raise AssertionError('login/logout resource leak')
    if original is not None and sha(working)!=original:raise AssertionError('working disk changed')
    print(f'PASS: {ram} exact production-seed login, permissions, messaging, passwd/su/newgrp ownership, flow/EOF, cold-boot credentials/data/RTC and four-session resource recovery; working disk preserved')


if __name__=='__main__':main()

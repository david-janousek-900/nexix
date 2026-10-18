"""Exercise V7 newgrp including the shell's exec-style builtin dispatch."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'newgrp',{'/bin/credentials':'credentials.elf'},2,single_user=False) as guest:
        guest.expect('login: ');guest.send('alice\n');guest.expect('Password:');guest.send('alice\n');guest.expect(re.escape('$ '))
        guest.expect('login: ',1);guest.send('root\n',1);guest.expect('Password:',1);guest.send('nexix\n',1);guest.expect('# ',1)
        guest.command('cd /',prompt=re.escape('$ '))
        def state(uid,gid,home,extra=''):
            guest.command('/bin/credentials'+extra,stdout=f'UID={uid} EUID={uid} GID={gid} EGID={gid} HOME={home} CWD_ROOT=1\n',prompt=re.escape('$ '))
        wire='/bin/credentials launch; echo LAUNCH_STATUS_$?'
        guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n$ '))
        state(100,100,'/home/alice',' closed');guest.send('\x04');guest.expect('LAUNCH_STATUS_0\r\n'+re.escape('$ '));guest.command('rm /tmp/newgrp-fd',prompt=re.escape('$ '))
        def group(name,message=''):
            wire='newgrp'+(' '+name if name else '')
            guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'+(message+'\r\n' if message else '')+'$ '))
        group('staff');state(100,100,'/home/alice')
        group('other');state(100,1,'/home/alice')
        group('bin','Sorry');state(100,1,'/home/alice')
        group('missing','missing: no such group');state(100,1,'/home/alice')
        group('','usage: newgrp groupname');state(100,1,'/home/alice')
        guest.command("echo 'plain::102:100:Plain:/:/bin/sh' >> /etc/passwd",1)
        guest.command("echo 'guarded:gp04XG.2NhSMM:105:plain' >> /etc/group",1)
        guest.send('\x04');guest.expect('login: ');guest.send('plain\n');guest.expect(re.escape('$ '))
        guest.send('newgrp guarded\n');guest.expect('Password:');guest.send('wrong\n');guest.expect('Sorry\r\n'+re.escape('$ '));state(102,100,'/')
        guest.send('newgrp guarded\n');guest.expect('Password:');start=len(guest.buffers[0]);guest.send('Group123\n');guest.expect(re.escape('$ '))
        if b'Group123' in guest.buffers[0][start:]:raise AssertionError('group password echoed')
        state(102,105,'/')
    print(f'PASS: {ram} newgrp membership/other, denial/usage replacement shells, real/effective GID, UID drop, inherited environment, fd19 close and group passwords')


if __name__=='__main__':main()

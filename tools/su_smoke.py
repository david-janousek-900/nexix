"""Exercise V7 su's preserved environment and real credential transitions."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'su',{'/bin/credentials':'credentials.elf'},2,single_user=False) as guest:
        guest.expect('login: ');guest.send('alice\n');guest.expect('Password:');guest.send('alice\n');guest.expect(re.escape('$ '))
        guest.command('cd /',prompt=re.escape('$ '))
        guest.command('su missing',stdout='Unknown id: missing\n',status=1,prompt=re.escape('$ '))
        guest.send('su\n');guest.expect('su\r\nPassword:');guest.send('wrong\n');guest.expect('Sorry\r\n'+re.escape('$ '))
        guest.send('su\n');guest.expect('su\r\nPassword:');start=len(guest.buffers[0]);guest.send('nexix\n');guest.expect('# ')
        if b'nexix' in guest.buffers[0][start:]:raise AssertionError('su password echoed')
        def state(uid,gid,home,prompt):
            guest.command('/bin/credentials',stdout=f'UID={uid} EUID={uid} GID={gid} EGID={gid} HOME={home} CWD_ROOT=1\n',prompt=re.escape(prompt))
        state(0,0,'/home/alice','# ')
        guest.send('su bob\n');guest.expect('su bob\r\n'+re.escape('$ '))
        state(101,100,'/home/alice','$ ')
        guest.send('\x04');guest.expect('# ')
        guest.command("echo 'bad::102:100:Bad:/:/missing' >> /etc/passwd")
        guest.command('su bad',stdout='No shell\n',status=3)
        guest.send('\x04');guest.expect(re.escape('$ '))
        state(100,100,'/home/alice','$ ')
    print(f'PASS: {ram} su unknown/wrong passwords, hidden input, root bypass, real/effective IDs, inherited HOME/cwd, nested shells and failed exec')


if __name__=='__main__':main()

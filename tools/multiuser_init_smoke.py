"""Boot the production init directly to getty, reload lines and recover."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'multiuser-init',{'/bin/initcontrol':'init-control.elf'},2,single_user=False) as guest:
        def login(name,password,line,prompt):
            guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line)
            guest.send(password+'\n',line);guest.expect(re.escape(prompt),line)
        login('root','nexix',0,'# ');login('bob','bob',1,'$ ')
        guest.command('/etc/init',stdout='init: must be pid 1\n',status=1)
        guest.command('TOKEN=KEPT',1,prompt=re.escape('$ '))
        guest.command('/bin/initcontrol two',stdout='INIT_STATE_OK\n')
        guest.command('/bin/initcontrol reload')
        guest.command('/bin/initcontrol two',stdout='INIT_STATE_OK\n')
        guest.command('echo $TOKEN',1,'KEPT\n',prompt=re.escape('$ '))
        guest.command("echo '19console' > /etc/ttys")
        guest.command('/bin/initcontrol reload')
        guest.command('/bin/initcontrol one',stdout='INIT_STATE_OK\n')
        guest.command("echo '19ttyS1' >> /etc/ttys")
        guest.command('/bin/initcontrol reload')
        login('bob','bob',1,'$ ')
        guest.command('/bin/initcontrol two',stdout='INIT_STATE_OK\n')
        for code in (126,127):
            guest.send(f"exec /bin/sh -c 'exit {code}'\n",1);login('bob','bob',1,'$ ')
            guest.command('/bin/initcontrol two',stdout='INIT_STATE_OK\n')
        guest.send('/bin/initcontrol single\n')
        guest.expect('init: single-user recovery\r\nroot@[A-Za-z0-9.-]+ # ')
        guest.command('/bin/initcontrol recovery',stdout='INIT_STATE_OK\n')
        guest.send('\x04')
        login('root','nexix',0,'# ');login('bob','bob',1,'$ ')
        guest.command('/bin/initcontrol two',stdout='INIT_STATE_OK\n')
        guest.send('\x04',1);guest.expect('login: ',1)
        guest.send('\x04',1);guest.expect('login: ',1)
        guest.command('echo INIT_SURVIVES',stdout='INIT_SURVIVES\n')
    print(f'PASS: {ram} automatic two-line login, distinct groups, unchanged-session reload, disable/enable, single-user recovery, multi-user return and EOT respawn')
    with private_guest(qemu,iso,ram,'multiuser-one-port',{'/bin/initcontrol':'init-control.elf'},1,single_user=False) as guest:
        guest.expect('login: ');guest.send('root\n');guest.expect('Password:');guest.send('nexix\n');guest.expect('# ')
        for _ in range(3):guest.command('/bin/initcontrol no-peer',stdout='INIT_STATE_OK\n')
    print(f'PASS: {ram} absent COM2 does not spin or prevent console login')


if __name__=='__main__':main()

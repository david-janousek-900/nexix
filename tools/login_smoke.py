"""Exercise installed login authentication, shell credentials and environment."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'login',{'/bin/identity':'login-identity.elf'}) as guest:
        guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
        guest.command("echo 'probe:alfu/zwrG7eZw:100:100:Probe:/home/alice:identity' >> /etc/passwd")
        guest.command("echo 'bad:alfu/zwrG7eZw:100:100:Bad:/missing:identity' >> /etc/passwd")
        def password(text):
            guest.expect('Password:');start=len(guest.buffers[0]);guest.send(text+'\n');return start
        def hidden(start,text):
            if text.encode() in guest.buffers[0][start:]:raise AssertionError('password echoed')
        guest.send('/bin/login probe\n');guest.expect('/bin/login probe\r\n')
        start=password('wrong');guest.expect('Login incorrect\r\nlogin: ');hidden(start,'wrong')
        guest.send('unknown\n');start=password('wrong');guest.expect('Login incorrect\r\nlogin: ');hidden(start,'wrong')
        guest.send('bad\n');start=password('alice');guest.expect('No directory\r\nlogin: ');hidden(start,'alice')
        guest.send('probe\n');start=password('alice');guest.expect('LOGIN_IDENTITY_OK\r\n# ');hidden(start,'alice')
        # The real shell must run in its own home with the unprivileged prompt.
        guest.send('/bin/login alice\n');guest.expect('/bin/login alice\r\n')
        start=password('alice');guest.expect('Nexix 0.1.0 development system\r\n'+re.escape('$ '));hidden(start,'alice')
        guest.command('echo $HOME',stdout='/home/alice\n',prompt=re.escape('$ '))
        guest.command('pwd',stdout='/home/alice\n',prompt=re.escape('$ '))
        guest.command('echo owned > mine; cat mine; rm mine',stdout='owned\n',prompt=re.escape('$ '))
        guest.send('\x04');guest.expect('# ')
        # Username EOF is a clean exit, not a blocked prompt or kernel failure.
        guest.send('/bin/login\n');guest.expect('/bin/login\r\nlogin: ')
        guest.send('\x04');guest.expect('# ')
        guest.command('echo LOGIN_PARENT_ALIVE',stdout='LOGIN_PARENT_ALIVE\n')
    print(f'PASS: {ram} login correct/wrong/unknown credentials, hidden passwords, missing home, UID/GID, cwd/HOME/PATH, native execlp, user shell and EOF')


if __name__=='__main__':main()

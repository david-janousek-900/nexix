"""Check the installed set-ID passwd command in real authenticated sessions."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'passwd-command',{'/bin/pwstate':'password-state.elf'},2,single_user=False) as guest:
        def login(password):
            guest.expect('login: ');guest.send('alice\n');guest.expect('Password:');guest.send(password+'\n')
            guest.expect(re.escape('$ '))
        login('alice')
        guest.expect('login: ',1);guest.send('root\n',1);guest.expect('Password:',1);guest.send('nexix\n',1);guest.expect('# ',1)
        guest.command('passwd root',0,'Permission denied.\n',1,prompt=re.escape('$ '))
        def change(old,new=None,again=None,line=0,message='',implicit=False):
            command='passwd' if implicit else 'passwd alice'
            guest.send(command+'; echo PASSWD_STATUS_$?\n',line)
            guest.expect(re.escape(command+'; echo PASSWD_STATUS_$?\r\n'),line)
            if implicit:guest.expect('Changing password for alice\r\n',line)
            def secret(prompt,text):
                guest.expect(prompt,line);start=len(guest.buffers[line]);guest.send(text+'\n',line);return start
            if old is not None:secret('Old password:',old)
            if new is not None:secret('New password:',new);secret('Retype new password:',again if again is not None else new)
            if message:guest.expect(re.escape(message+'\r\n'),line)
            guest.expect('PASSWD_STATUS_1\r\n'+r'[A-Za-z0-9_.-]+@[A-Za-z0-9.-]+'+re.escape('# ' if line else '$ '),line)
        change('wrong',message='Sorry.',implicit=True)
        change('alice','Newpass1','Mismatch',message='Mismatch - password unchanged.')
        guest.command('echo busy > /etc/ptmp',1)
        change('alice','Newpass1',message='Temporary file busy -- try again')
        guest.command('cat /etc/ptmp; rm /etc/ptmp',1,'busy\n')
        guest.command('echo backup > /etc/opasswd',1)
        change('alice','Newpass1',message='Cannot preserve or update password file.')
        guest.command('cat /etc/opasswd; rm /etc/opasswd',1,'backup\n')
        change('alice','Newpass1',implicit=True)
        guest.send('\x04')
        guest.expect('login: ');guest.send('alice\n');guest.expect('Password:');guest.send('alice\n')
        guest.expect('Login incorrect\r\nlogin: ');guest.send('alice\n');guest.expect('Password:');guest.send('Newpass1\n');guest.expect(re.escape('$ '))
        change(None,'Reset123',line=1)
        guest.command('/bin/pwstate',1,'PASSWD_STATE_OK\n')
        guest.send('\x04');login('Reset123')
        guest.command('echo PASSWORD_LOGIN_OK',stdout='PASSWORD_LOGIN_OK\n',prompt=re.escape('$ '))
        for output in guest.buffers:
            if b'Newpass1' in output or b'Reset123' in output:raise AssertionError('new password echoed')
    print(f'PASS: {ram} set-ID passwd authorization, old-password checks, mismatch, exclusive lock/backup, updates, root reset, modes and new-password login')


if __name__=='__main__':main()

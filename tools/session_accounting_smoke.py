"""Check actual getty/login/init record writes for two authenticated sessions."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'session-accounting',{'/bin/sessioncheck':'session-records.elf'},2) as guest:
        guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+ # ')
        guest.command("echo '19console' > /etc/ttys; echo '19ttyS1' >> /etc/ttys")
        guest.send('\x04')
        def login(name,password,line,prompt):
            guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line)
            guest.send(password+'\n',line);guest.expect(re.escape(prompt),line)
        login('alice','alice',0,'$ ');login('bob','bob',1,'$ ')
        guest.command('/bin/sessioncheck 0',0,'SESSION_RECORDS_OK\n',prompt=re.escape('$ '))
        guest.send('\x04',0)
        guest.expect('login: ',0)
        guest.command('/bin/sessioncheck 1',1,'SESSION_RECORDS_OK\n',prompt=re.escape('$ '))
        guest.send('root\n');guest.expect('Password:');guest.send('nexix\n');guest.expect('# ')
        guest.send('\x04',1);guest.expect('login: ',1)
        guest.command('/bin/sessioncheck 2',0,'SESSION_RECORDS_OK\n')
    print(f'PASS: {ram} actual two-user login/logout utmp slots, append-only wtmp history, RTC times, peer-session preservation and getty respawn')


if __name__=='__main__':main()

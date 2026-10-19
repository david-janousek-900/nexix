"""Check V7 who against live accounting and fixed-width native records."""
import re
import struct
import sys
from private_guest import private_guest
from install_manifest import ROOT


def main():
    qemu,iso,ram=sys.argv[1:4]
    (ROOT/'build/who-records.bin').write_bytes(struct.pack('<8s8sq',b'LINE1234',b'USER1234',1709210096))
    with private_guest(qemu,iso,ram,'who',{'/tmp/records':'who-records.bin'},2,single_user=False) as guest:
        for line,name in enumerate(('alice','bob')):
            guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line);guest.send(name+'\n',line);guest.expect(re.escape('$ '),line)
        def row(name,line):return re.escape(f'{name:<8} {line:<8}')+r'Feb 29 12:[0-5][0-9]\r\n'
        def query(command,pattern,line=0):
            wire=command+'; echo WHO_STATUS_$?'
            guest.send(wire+'\n',line);guest.expect(re.escape(wire+'\r\n'),line)
            guest.expect(pattern+'WHO_STATUS_0\r\n'+r'[A-Za-z0-9_.-]+@[A-Za-z0-9.-]+ '+re.escape('$ '),line)
        query('who',row('alice','console')+row('bob','ttyS1'))
        query('who am i',row('alice','console'));query('who am i',row('bob','ttyS1'),1)
        query('who am i < /etc/rc',row('alice','tty??'))
        query('who /etc/utmp',re.escape(' '*17+'Jan  1 00:00\r\n')+row('alice','console')+row('bob','ttyS1'))
        query('who /tmp/records',row('USER1234','LINE1234'))
        guest.command('who /missing',stdout='who: cannot open utmp\n',status=1,prompt=re.escape('$ '))
        guest.send('\x04');guest.expect('login: ')
        query('who',row('bob','ttyS1'),1)
    print(f'PASS: {ram} who live users, each terminal identity, no-TTY fallback, explicit native files, nonterminated eight-byte fields, errors and logout')


if __name__=='__main__':main()

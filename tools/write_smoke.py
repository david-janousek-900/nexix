"""Exchange real V7 write messages between authenticated serial sessions."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'write',{},2,single_user=False) as guest:
        for line,name in enumerate(('alice','bob')):
            guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line);guest.send(name+'\n',line);guest.expect(re.escape('$ '),line)
        guest.command('write',stdout='usage: write user [ttyname]\n',status=1,prompt=re.escape('$ '))
        guest.command('write missing',stdout='missing not logged in.\n',status=1,prompt=re.escape('$ '))
        guest.command('write bob ../passwd',stdout='Invalid tty name\n',status=1,prompt=re.escape('$ '))
        guest.command('mesg n',1,status=1,prompt=re.escape('$ '))
        guest.command('write bob',stdout='Permission denied\n',status=1,prompt=re.escape('$ '))
        guest.command('mesg y',1,prompt=re.escape('$ '))
        wire='write bob; echo WRITE_STATUS_$?'
        guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'))
        guest.expect(re.escape('Message from alice console...\r\n\x07\x07\x07'),1)
        guest.send('ALICE_MESSAGE\n');guest.expect('ALICE_MESSAGE\r\n',1)
        escape='!echo '+'Q'*121+'\n';assert len(escape)==128
        guest.send(escape);guest.expect(re.escape('Q'*121+'\r\n!\r\n'))
        guest.send('\x04');guest.expect('EOF\r\n',1);guest.expect('WRITE_STATUS_0\r\n'+re.escape('$ '))
        if b'Q'*121 in guest.buffers[1]:raise AssertionError('shell escape forwarded')
        guest.send('write bob ttyS1\n');guest.expect('write bob ttyS1\r\n')
        guest.expect(re.escape('Message from alice console...\r\n\x07\x07\x07'),1)
        guest.send('EXPLICIT_MESSAGE\n');guest.expect('EXPLICIT_MESSAGE\r\n',1)
        guest.send('\x03');guest.expect('EOF\r\n',1);guest.expect(re.escape('$ '))
        guest.command('echo SENDER_ALIVE',stdout='SENDER_ALIVE\n',prompt=re.escape('$ '))
        guest.command('echo RECEIVER_ALIVE',1,'RECEIVER_ALIVE\n',prompt=re.escape('$ '))
    print(f'PASS: {ram} write usage/lookup/permissions, two-user delivery, explicit tty, bounded 128-byte shell escape, EOF/SIGINT and both live shells')


if __name__=='__main__':main()

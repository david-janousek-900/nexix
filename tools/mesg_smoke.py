"""Check retained mesg statuses, per-line modes and real permission denial."""
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'mesg',{'/tmp/mesgcheck':'mesg-command.elf'},2) as guest:
        guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+ # ')
        guest.command('mesg',stdout='is n\n',status=1)
        guest.command('mesg y');guest.command('mesg',stdout='is y\n')
        guest.command('mesg n',status=1);guest.command('mesg',stdout='is n\n',status=1)
        guest.command('mesg invalid',stdout='mesg: usage: mesg [y] [n]\n',status=255)
        guest.command('mesg 2>/tmp/mesgerr',status=255)
        guest.command('cat /tmp/mesgerr',stdout='mesg: cannot stat\n');guest.command('rm /tmp/mesgerr')
        guest.command('mesg y 2>/dev/ttyS1')
        guest.command('mesg 2>/dev/ttyS1');guest.expect('is y\r\n',1)
        guest.command('mesg',stdout='is n\n',status=1)
        guest.command('mesg n 2>/dev/ttyS1',status=1)
        guest.command('/tmp/mesgcheck',stdout='MESG_PERMISSIONS_OK\n')
        guest.expect('mesg: cannot change mode\r\n',1)
    print(f'PASS: {ram} mesg modes/statuses, independent terminals, invalid/non-TTY input and non-owner denial')


if __name__=='__main__':main()

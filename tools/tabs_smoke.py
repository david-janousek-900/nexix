"""Installed native V7 tabs acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
stops=lambda n:(b' '*8+b'\0331')*n
Path('build/tabs-300').write_bytes(b'\0332'+b'\177'*8+b'\r\n'+stops(14)+b'\n')
Path('build/tabs-hp').write_bytes(b'\0333\r'+stops(9)+b'\n')
with private_guest(qemu,iso,ram,'tabs',{'/tmp/t300':'tabs-300','/tmp/hp':'tabs-hp'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('tabs 300 | cmp - /tmp/t300')
    guest.command('tabs hp | cmp - /tmp/hp')
    guest.command('tabs </dev/null | wc -c',stdout='      0\n')
    guest.command('tabs tn300 > /dev/null; echo ok',stdout='ok\n')
print(f'PASS: {ram} tabs terminal sequences, no-terminal detection and tn300 terminal settings')

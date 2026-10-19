"""Installed V7 tail acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
lines=''.join(f'{i:04}\n' for i in range(2000))
Path('build/tail-lines').write_text(lines)
Path('build/tail-short').write_text('one\ntwo\nthree')
with private_guest(qemu,iso,ram,'tail',{'/tmp/lines':'tail-lines','/tmp/short':'tail-short'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('tail /tmp/lines',stdout=lines[-50:])
    guest.command('tail -3 /tmp/lines',stdout=lines[-15:])
    guest.command('cat /tmp/lines | tail -3',stdout=lines[-15:])
    guest.command('tail +1999l /tmp/lines',stdout=lines[-10:])
    guest.command('tail -1b /tmp/lines',stdout=lines[-512:])
    guest.command('tail +9999c /tmp/lines',stdout=lines[9998:])
    guest.command('tail -r /tmp/short',stdout='three\ntwo\none\n')
    guest.command('tail +1l /tmp/short; echo',stdout='one\ntwo\nthree\n')
    guest.command('tail -1c /tmp/short; echo',stdout='e\n')
    guest.command('tail -0 /tmp/lines')
    guest.command('tail /tmp/missing',stdout="tail: can't open /tmp/missing\n",status=1)
    guest.command('tail -999999999999999999999c /tmp/lines',stdout='usage: tail [+|-number[lbcr]] [file]\n',status=1)
print(f'PASS: {ram} tail default, lines/bytes/blocks, reverse, files/pipes and errors')

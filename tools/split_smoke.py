"""Installed native V7 split acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/split-input').write_text(''.join(f'{i}\n' for i in range(2500)))
with private_guest(qemu,iso,ram,'split',{'/tmp/input':'split-input'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('cd /tmp; split input; echo xa?',stdout='xaa xab xac\n')
    guest.command('cat xaa xab xac | cmp - input; wc -l xac',stdout='    500 xac\n')
    guest.command('split -1000 - part < input; echo part*',stdout='partaa partab partac\n')
    guest.command('split -0',stdout='usage: split [-n] [file [name]]\n',status=1)
    guest.command('split /tmp/missing',stdout='cannot open input\n',status=1)
print(f'PASS: {ram} split counts, names, stdin, reconstruction and errors')

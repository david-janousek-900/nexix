"""Installed native V7 col acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/col-input').write_bytes(b'\n  down\x0b\rup\n\na\bb\n        x\n')
Path('build/col-expect').write_bytes(b'up\n  down\na\bb\n\tx\n')
Path('build/col-b').write_bytes(b'up\n  down\nb\n        x\n')
with private_guest(qemu,iso,ram,'col',{'/tmp/input':'col-input','/tmp/expect':'col-expect','/tmp/b':'col-b'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('col < /tmp/input | cmp - /tmp/expect')
    guest.command('col -bx < /tmp/input | cmp - /tmp/b')
    guest.command('col -q </dev/null',stdout='col: bad option letter q\n',status=2)
print(f'PASS: {ram} col reverse feeds, overstrikes, tabs, flags and errors')

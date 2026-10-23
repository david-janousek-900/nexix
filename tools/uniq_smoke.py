"""Installed native V7 uniq acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/uniq-input').write_text('a\na\nb\nc\nc\nc\nd\n')
with private_guest(qemu,iso,ram,'uniq',{'/tmp/input':'uniq-input'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('uniq /tmp/input',stdout='a\nb\nc\nd\n')
    guest.command('uniq -c < /tmp/input',stdout='   2 a\n   1 b\n   3 c\n   1 d\n')
    guest.command('uniq -d /tmp/input; uniq -u /tmp/input',stdout='a\nc\nb\nd\n')
    guest.command('uniq /tmp/input /tmp/out; cat /tmp/out',stdout='a\nb\nc\nd\n')
    guest.command('uniq /tmp/missing',stdout='cannot open /tmp/missing\n',status=1)
print(f'PASS: {ram} uniq modes, files, output file and errors')

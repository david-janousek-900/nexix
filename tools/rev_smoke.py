"""Installed native V7 rev acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
long='x'*300+'y'
Path('build/rev-text').write_text(f'abc\n\n{long}\nend')
with private_guest(qemu,iso,ram,'rev',{'/tmp/text':'rev-text'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('rev /tmp/text',stdout=f'cba\n\n{long[::-1]}\ndne\n')
    guest.command('echo hello | rev',stdout='olleh\n')
    guest.command('rev /tmp/missing',stdout='rev: cannot open /tmp/missing\n',status=1)
print(f'PASS: {ram} rev files, pipes, 301-byte and unterminated lines and errors')

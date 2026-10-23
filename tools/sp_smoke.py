"""Installed native V7 sp acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/sp-input').write_text('a\nbb\nccc\n')
Path('build/sp-expect').write_bytes(b'a\tbb\tccc\n')
Path('build/sp-narrow').write_bytes(b'aaaa\nbbbb\ncccc\n')
with private_guest(qemu,iso,ram,'sp',{'/tmp/input':'sp-input','/tmp/expect':'sp-expect','/tmp/narrow':'sp-narrow'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('sp < /tmp/input | cmp - /tmp/expect')
    guest.command('sp 10 < /tmp/narrow',stdout='aaaa\nbbbb\ncccc\n')
print(f'PASS: {ram} sp tab packing and widths')

"""Installed native V7 tr acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/tr-bytes').write_bytes(bytes(range(1,256)))
with private_guest(qemu,iso,ram,'tr',{'/tmp/bytes':'tr-bytes'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('echo hello, world | tr a-z A-Z',stdout='HELLO, WORLD\n')
    guest.command("echo hello, world | tr -cs a-z '\\012'",stdout='hello\nworld\n')
    guest.command('echo hello | tr -d l',stdout='heo\n')
    guest.command("tr '\\001-\\377' '\\001-\\377' < /tmp/bytes | cmp - /tmp/bytes")
    guest.command("tr -d '\\001-\\377' < /tmp/bytes | wc -c",stdout='      0\n')
print(f'PASS: {ram} tr ranges, complement/squeeze/delete and all byte values')

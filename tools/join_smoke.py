"""Installed native V7 join acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/join-a').write_text('a 1\nb 2\nb 3\nd 4\n')
Path('build/join-b').write_text('b x\nb y\nc z\nd w\n')
with private_guest(qemu,iso,ram,'join',{'/tmp/a':'join-a','/tmp/b':'join-b'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('join /tmp/a /tmp/b',stdout='b 2 x\nb 2 y\nb 3 x\nb 3 y\nd 4 w\n')
    guest.command('join -a1 -e NONE -o 1.1 2.2 /tmp/a /tmp/b',stdout='a NONE\nb x\nb y\nb x\nb y\nd w\n')
    guest.command('cat /tmp/a | join -a2 - /tmp/b | grep c',stdout='c z\n')
    guest.command('join /tmp/a',stdout='join: usage: join [-j1 x -j2 y] [-o list] file1 file2\n',status=1)
    guest.command('join /tmp/a /tmp/missing',stdout="join: can't open /tmp/missing\n",status=1)
print(f'PASS: {ram} join matches, -a/-e/-o, stdin and errors')

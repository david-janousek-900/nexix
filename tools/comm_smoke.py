"""Installed native V7 comm acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/comm-a').write_text('apple\nbanana\ncherry\nfig\n')
Path('build/comm-b').write_text('banana\ncherry\ndate\n')
Path('build/comm-all').write_bytes(b'apple\n\t\tbanana\n\t\tcherry\n\tdate\nfig\n')
with private_guest(qemu,iso,ram,'comm',{'/tmp/a':'comm-a','/tmp/b':'comm-b','/tmp/all':'comm-all'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('comm /tmp/a /tmp/b | cmp - /tmp/all')
    guest.command('comm -12 /tmp/a /tmp/b',stdout='banana\ncherry\n')
    guest.command('cat /tmp/a | comm -23 - /tmp/b',stdout='apple\nfig\n')
    guest.command('comm -4 /tmp/a /tmp/b',stdout='comm: illegal flag\n',status=1)
    guest.command('comm /tmp/a',stdout='comm: arg count\n',status=1)
    guest.command('comm /tmp/a /tmp/missing',stdout='comm: cannot open /tmp/missing\n',status=1)
print(f'PASS: {ram} comm columns, flags, stdin and errors')

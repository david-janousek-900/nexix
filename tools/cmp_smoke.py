"""Installed binary comparator acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
files={'a':b'alpha\nbeta\n','b':b'alpha\nBeta\n','short':b'alpha\n','prefix':b'0'*8+b'alpha\nbeta\n','binary':bytes(range(256))*20}
artifacts={}
for name,data in files.items():
    artifact='cmp-'+name;Path('build',artifact).write_bytes(data);artifacts['/tmp/'+name]=artifact
with private_guest(qemu,iso,ram,'cmp',artifacts,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('cmp /tmp/a /tmp/a')
    guest.command('cmp /tmp/binary /tmp/binary')
    guest.command('cat /tmp/a | cmp - /tmp/a')
    guest.command('cmp /tmp/a /tmp/b',stdout='/tmp/a /tmp/b differ: char 7, line 2\n',status=1)
    guest.command('cmp -l /tmp/a /tmp/b',stdout='     7 142 102\n',status=1)
    guest.command('cmp -s /tmp/a /tmp/b',status=1)
    guest.command('cmp /tmp/a /tmp/short',stdout='cmp: EOF on /tmp/short\n',status=1)
    guest.command('cmp -s /tmp/a /tmp/short',status=1)
    guest.command('cmp /tmp/a /tmp/prefix 0 010')
    guest.command('cmp /tmp/a /tmp/b 08',stdout='cmp: invalid skip\n',status=2)
    guest.command('cmp /tmp/a /tmp/missing',stdout='cmp: cannot open /tmp/missing\n',status=2)
    guest.command('cmp -s /tmp/a /tmp/missing',status=2)
print(f'PASS: {ram} cmp binary equality, differences, octal list, skip, stdin and statuses')

"""Installed native V7 sort acceptance, including temporary-file merges."""
from pathlib import Path
import random
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
rng=random.Random(8)
lines=[f'{rng.randrange(10**9):09}' for _ in range(150000)]
Path('build/sort-big').write_text('\n'.join(lines)+'\n')
Path('build/sort-expect').write_text('\n'.join(sorted(lines))+'\n')
Path('build/sort-keys').write_text('a 3\nb 1\nc 2\nb 1\n')
with private_guest(qemu,iso,ram,'sort',{'/tmp/big':'sort-big','/tmp/expect':'sort-expect','/tmp/keys':'sort-keys'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('sort +1n /tmp/keys',stdout='b 1\nb 1\nc 2\na 3\n')
    guest.command('sort -u -r /tmp/keys',stdout='c 2\nb 1\na 3\n')
    guest.command('sort -c /tmp/keys',stdout='sort: disorder:b 1\n',status=1)
    guest.command('sort /tmp/big | cmp - /tmp/expect')
    guest.command('sort -o /tmp/sorted /tmp/big; cmp /tmp/sorted /tmp/expect; ls /tmp | grep -c stm',stdout='0\n',status=1)
    guest.command('sort -m /tmp/expect /tmp/keys | wc -l',stdout=' 150004\n')
    guest.command('sort /tmp/missing',stdout="sort: can't open /tmp/missing\n",status=1)
print(f'PASS: {ram} sort keys, -u/-r/-c/-m/-o, 1.5 MB temporary-file merge and errors')

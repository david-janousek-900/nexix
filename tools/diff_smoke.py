"""Installed native V7 diff, /usr/lib/diffh, /usr/lib/diff3 and the diff3 script."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/diff-f1').write_text('a\nb\nc\nd\n')
Path('build/diff-f2').write_text('a\nx\nc\nd\ne\n')
Path('build/diff-o2').write_text('a\nB\nc\nd\n')
Path('build/diff-o3').write_text('a\nb\nc\nD\n')
big=''.join(f'line {i}\n' for i in range(5000))
Path('build/diff-big1').write_text(big)
Path('build/diff-big2').write_text(big.replace('line 1234\n','changed\n'))
files={'/tmp/f1':'diff-f1','/tmp/f2':'diff-f2','/tmp/o2':'diff-o2','/tmp/o3':'diff-o3','/tmp/big1':'diff-big1','/tmp/big2':'diff-big2'}
with private_guest(qemu,iso,ram,'diff',files,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('diff /tmp/f1 /tmp/f2',stdout='2c2\n< b\n---\n> x\n4a5\n> e\n',status=1)
    guest.command('diff -e /tmp/f1 /tmp/f2',stdout='4a\ne\n.\n2c\nx\n.\n',status=1)
    guest.command('diff /tmp/f1 /tmp/f1',status=0)
    guest.command('cat /tmp/f1 | diff - /tmp/f2 | wc -l',stdout='      6\n')
    guest.command('diff /tmp/big1 /tmp/big2',stdout='1235c1235\n< line 1234\n---\n> changed\n',status=1)
    guest.command('(diff -e /tmp/f1 /tmp/f2; echo w /tmp/e1; echo q) | ed - /tmp/f1; cmp /tmp/e1 /tmp/f2')
    guest.command('diff -h /tmp/f1 /tmp/f2',stdout='2,$c2,$\n< b\n< c\n< d\n---\n> x\n> c\n> d\n> e\n',status=1)
    guest.command('/usr/lib/diffh /tmp/big1 /tmp/big2',stdout='1235c1235\n< line 1234\n---\n> changed\n',status=1)
    guest.command('diff3 /tmp/f1 /tmp/o2 /tmp/o3',stdout='====2\n1:2c\n2:2c\n  B\n3:2c\n  b\n====3\n1:4c\n2:4c\n  d\n3:4c\n  D\n')
    guest.command('diff3 -e /tmp/f1 /tmp/o2 /tmp/o3',stdout='4c\nD\n.\n')
    guest.command('ls /tmp | grep -c d3',stdout='0\n',status=1)
    guest.command('diff3 /tmp/f1 /tmp/o2',stdout='test: argument expected\nusage: diff3 file1 file2 file3\n')
    guest.command('diff3 /tmp/f1 /tmp/o2 /tmp/missing',stdout='usage: diff3 file1 file2 file3\n')
    guest.command('diff /tmp/f1 /tmp/missing',stdout='diff: cannot open /tmp/missing\n',status=2)
print(f'PASS: {ram} diff normal/-e/stdin/large, ed round trip, -h through diffh, diff3 script and helper, errors')

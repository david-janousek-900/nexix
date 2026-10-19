"""Installed native V7 wc acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/wc-text').write_bytes(b'one two\nthree\n')
Path('build/wc-binary').write_bytes(b'a\0b\v\xff c\n')
with private_guest(qemu,iso,ram,'wc',{'/tmp/text':'wc-text','/tmp/binary':'wc-binary'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('wc /tmp/text',stdout=f'{2:7}{3:7} {14:7} /tmp/text\n')
    guest.command('cat /tmp/text | wc -cwl',stdout=f'{14:7}{3:7} {2:7}\n')
    guest.command('wc /tmp/binary',stdout=f'{1:7}{2:7} {8:7} /tmp/binary\n')
    guest.command('wc -l /tmp/text /tmp/text',stdout=f'{2:7} /tmp/text\n'*2+f'{4:7} total\n')
    guest.command('wc -l /tmp/missing /tmp/text',stdout="wc: can't open /tmp/missing\n"+f'{2:7} /tmp/text\n{2:7} total\n',status=1)
    guest.command('wc -z /tmp/text',stdout='wc: unknown option\n',status=1)
print(f'PASS: {ram} wc lines/words/bytes, V7 binary classification, option order, pipes, totals and errors')

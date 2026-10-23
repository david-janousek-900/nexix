"""Installed native V7 pr acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/pr-text').write_text(''.join(f'line {i}\n' for i in range(1,131)))
Path('build/pr-two').write_text('a\nb\n')
cols=''.join(f'line {i}\t\tline {i+3}\n' for i in range(1,4))
with private_guest(qemu,iso,ram,'pr',{'/tmp/text':'pr-text','/tmp/two':'pr-two'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('pr /tmp/text | wc -l',stdout='    198\n')
    guest.command('pr -h Title /tmp/two | grep Page | wc -l',stdout='      1\n')
    guest.command('pr -t -l3 /tmp/two',stdout='a\nb\n\n')
    guest.command('pr -t -m -l2 /tmp/two /tmp/two',stdout='a\t\t\t\t    a\nb\t\t\t\t    b\n\n\n')
    guest.command('pr +3 -t -l50 /tmp/text | grep -c line',stdout='30\n')
    guest.command('pr /tmp/missing; echo $?',stdout='1\n')
    guest.command('pr /tmp/missing >/tmp/out 2>&1; cat /tmp/out',stdout="pr: can't open /tmp/missing\n")
print(f'PASS: {ram} pr pagination, headers, -t/-l/-m/+page and errors')

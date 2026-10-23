"""Installed native V7 tsort acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/tsort-graph').write_text('a b\nb c\nc d\n')
Path('build/tsort-cycle').write_text('a b b c c b c d\n')
with private_guest(qemu,iso,ram,'tsort',{'/tmp/graph':'tsort-graph','/tmp/cycle':'tsort-cycle'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('tsort /tmp/graph',stdout='a\nb\nc\nd\n')
    guest.command('tsort < /tmp/cycle 2>/dev/null',stdout='a\nb\nc\nd\n')
    guest.command('tsort /tmp/cycle >/dev/null',stdout='tsort: cycle in reverse order\ntsort: b\ntsort: c\n')
    guest.command('echo a b c | tsort',stdout='tsort: odd data\n',status=1)
    guest.command('tsort /tmp/missing',stdout='tsort: cannot open /tmp/missing\n',status=1)
print(f'PASS: {ram} tsort ordering, cycles, odd data and errors')

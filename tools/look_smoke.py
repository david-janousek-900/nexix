"""Installed native V7 look acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/look-words').write_text(''.join(f'w{i:05}\n' for i in range(3000)))
Path('build/look-fold').write_text('Apple\napricot\nBanana\nban-dit\nbanjo\n')
with private_guest(qemu,iso,ram,'look',{'/tmp/words':'look-words','/tmp/fold':'look-fold'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('look w0299 /tmp/words | wc -l',stdout='     10\n')
    guest.command('look w02999 /tmp/words',stdout='w02999\n')
    guest.command('look -df BAN /tmp/fold',stdout='Banana\nban-dit\nbanjo\n')
    guest.command('look word /tmp/missing',stdout="look: can't open /tmp/missing\n",status=2)
print(f'PASS: {ram} look search, -d/-f and errors')

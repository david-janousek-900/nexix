"""Installed native V7 fgrep acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/fgrep-text').write_text('apple pie\nbanana split\ncherry tart\napple crumble\nplain\n')
Path('build/fgrep-words').write_text(''.join(f'word{i}\n' for i in range(3000)))
with private_guest(qemu,iso,ram,'fgrep',{'/tmp/text':'fgrep-text','/tmp/words':'fgrep-words'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('fgrep apple /tmp/text',stdout='apple pie\napple crumble\n')
    guest.command('fgrep -x -n plain /tmp/text',stdout='5:plain\n')
    guest.command('fgrep -c -v apple /tmp/text',stdout='3\n')
    guest.command('fgrep -l tart /tmp/text /tmp/words',stdout='/tmp/text\n')
    guest.command('echo word2999 | fgrep -f /tmp/words',stdout='word2999\n')
    guest.command('fgrep zzz /tmp/text',status=1)
    guest.command('fgrep a /tmp/missing',stdout="fgrep: can't open /tmp/missing\n",status=2)
print(f'PASS: {ram} fgrep keywords, -x/-n/-c/-v/-l, 3000-word lists and errors')

"""Boot the installed grep and verify V7 flags, regex and errors."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login

qemu,iso,ram=sys.argv[1:4]
Path('build/grep-text').write_text('alpha\nbeta\nALPHA\nabba\n')
with private_guest(qemu,iso,ram,'grep',{'/tmp/text':'grep-text'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('grep alpha /tmp/text',stdout='alpha\n')
    guest.command('grep -y alpha /tmp/text',stdout='alpha\nALPHA\n')
    guest.command('grep -n -v alpha /tmp/text',stdout='2:beta\n3:ALPHA\n4:abba\n')
    guest.command('grep -c a /tmp/text',stdout='3\n')
    guest.command('grep -l a /tmp/text',stdout='/tmp/text\n')
    guest.command('grep -s a /tmp/text')
    guest.command('grep -b alpha /tmp/text',stdout='0:alpha\n')
    guest.command('grep -h alpha /tmp/text /tmp/text',stdout='alpha\nalpha\n')
    guest.command('grep alpha /tmp/text /tmp/text',stdout='/tmp/text:alpha\n/tmp/text:alpha\n')
    guest.command("grep '^\\(a\\)bb\\1$' /tmp/text",stdout='abba\n')
    guest.command("grep '^\\(\\)\\1*$' /tmp/text",status=1)
    guest.command('echo alpha | grep alpha',stdout='alpha\n')
    guest.command('grep absent /tmp/text',status=1)
    guest.command('grep a /tmp/missing',stdout="grep: can't open /tmp/missing\n",status=2)
    guest.command("grep '[bad' /tmp/text",stdout='grep: RE error\n',status=2)
print(f'PASS: {ram} grep installed flags, files/pipes, BRE/backreferences and error statuses')

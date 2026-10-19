"""Verify compact ls groups and explicit single-line-per-entry output."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/ls-content').write_text('contents\n')
with private_guest(qemu,iso,ram,'ls',{'/tmp/alpha':'ls-content','/tmp/beta':'ls-content'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('mkdir /tmp/dir /tmp/empty')
    guest.command('cd /tmp')
    guest.command('ls',stdout='alpha beta dir empty\n')
    guest.command('ls -1',stdout='alpha\nbeta\ndir\nempty\n')
    guest.command('ls -r',stdout='empty dir beta alpha\n')
    guest.command('ls -d dir empty',stdout='dir empty\n')
    guest.command('ls alpha beta',stdout='alpha beta\n')
    guest.command('ls empty')
    guest.command('ls dir empty',stdout='\ndir:\n\nempty:\n')
    guest.command('ls alpha dir',stdout='alpha\n\ndir:\n')
    guest.command('ls > /tmp/listing')
    guest.command('cat /tmp/listing',stdout='alpha beta dir empty listing\n')
    guest.send('ls -l alpha beta\n')
    guest.expect(r'alpha\r\n.*beta\r\n# ')
print(f'PASS: {ram} ls compact defaults, files/directories, empty/multiple groups, -1, reverse, redirection and long listing')

"""Verify compact ls groups and explicit single-line-per-entry output."""
from pathlib import Path
import sys
import re
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
    guest.command('chgrp staff alpha dir')
    guest.command('chown alice dir')
    guest.command('chown 65534 beta')
    guest.command('chgrp 65533 beta')
    def long_listing(command):
        guest.send(command+'\n');guest.expect(re.escape(command+'\r\n'))
        output,_=guest.expect(r'root@[A-Za-z0-9.-]+# ')
        return output.replace(b'\r\n',b'\n').decode()
    output=long_listing('ls -ld alpha beta dir')
    rows=output.splitlines()
    assert len(rows)==3,output
    for row,mode,owner,group,name in zip(rows,['-','-','d'],['root','65534','alice'],['staff','65533','staff'],['alpha','beta','dir']):
        fields=row.split()
        assert fields[0].startswith(mode) and fields[2:4]==[owner,group] and fields[-1]==name,row
    output=long_listing('ls -lg alpha beta')
    rows=output.splitlines()
    assert len(rows)==2 and rows[0].split()[2]=='staff' and rows[1].split()[2]=='65533',output
    # The same group must be emitted on every entry, even after lookup misses.
    output=long_listing('ls -ld alpha beta dir alpha')
    assert [row.split()[3] for row in output.splitlines()]==['staff','staff','65533','staff'],output
print(f'PASS: {ram} ls compact and long formats, owner/group names, high numeric ID fallback, directories and -g')

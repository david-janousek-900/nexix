"""Installed native V7 egrep acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/egrep-text').write_text('apple pie\nbanana split\ncherry tart\napple crumble\nplain\n')
Path('build/egrep-pats').write_text('pie\ntart\n')
Path('build/egrep-long').write_text('x'*3000+'needle\nhay\n')
with private_guest(qemu,iso,ram,'egrep',{'/tmp/text':'egrep-text','/tmp/pats':'egrep-pats','/tmp/long':'egrep-long'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command("egrep '^(apple|plain)' /tmp/text",stdout='apple pie\napple crumble\nplain\n')
    guest.command("egrep -n 'an(an)*a' /tmp/text",stdout='2:banana split\n')
    guest.command("egrep -c -v 'a' /tmp/text",stdout='0\n',status=1)
    guest.command('egrep -f /tmp/pats /tmp/text',stdout='apple pie\ncherry tart\n')
    guest.command('cat /tmp/text | egrep -f /tmp/pats',stdout='apple pie\ncherry tart\n')
    guest.command('egrep -c ne+dle /tmp/long',stdout='1\n')
    guest.command("egrep '(a' /tmp/text",stdout='egrep: syntax error\n',status=2)
    guest.command('egrep a /tmp/missing',stdout="egrep: can't open /tmp/missing\n",status=2)
print(f'PASS: {ram} egrep alternation, anchors, closures, pattern files, long lines and errors')

"""Installed native V7 sed acceptance."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/sed-text').write_text('one\ntwo\nthree\nfour\nfive\n')
Path('build/sed-script').write_text('#n\n/t/{\ns/t/T/g\np\n}\n')
with private_guest(qemu,iso,ram,'sed',{'/tmp/text':'sed-text','/tmp/script':'sed-script'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command("sed 's/o/0/g' /tmp/text",stdout='0ne\ntw0\nthree\nf0ur\nfive\n')
    guest.command("sed -n '2,4p' /tmp/text",stdout='two\nthree\nfour\n')
    guest.command("sed '1!G;h;$!d' /tmp/text",stdout='five\nfour\nthree\ntwo\none\n')
    guest.command("sed -n '/^f/=' /tmp/text",stdout='4\n5\n')
    guest.command('sed -f /tmp/script /tmp/text',stdout='Two\nThree\n')
    guest.command("cat /tmp/text | sed -e ':a' -e 's/^o//' -e 'ta' -e 3q",stdout='ne\ntwo\nthree\n')
    guest.command("sed 's/\\(.\\)\\(.\\)/\\2\\1/' /tmp/text | sed -n '$p'",stdout='ifve\n')
    guest.command("sed '/two/w /tmp/out' /tmp/text >/dev/null; cat /tmp/out",stdout='two\n')
    guest.command("sed 's/x' /tmp/text",stdout='command garbled: s/x\n',status=2)
    guest.command("sed p /tmp/missing",stdout="Can't open /tmp/missing\n",status=2)
print(f'PASS: {ram} sed substitutions, addresses, hold space, scripts, branches, w files and errors')

"""Run the exact native assembler first pass in a private acceptance consumer."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-as');(f/'bad.s').write_text('x:\nx:\n')
artifacts={'/tmp/asparse':'as-parse-test.elf','/tmp/frontend.s':'fixtures-as/frontend.s','/tmp/bad.s':'fixtures-as/bad.s'}
with private_guest(qemu,iso,ram,'as-parse',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    expected=(f/'expected').read_text()
    guest.command('/tmp/asparse /tmp/frontend.s',stdout=expected)
    guest.command('/tmp/asparse /tmp/bad.s',stdout='as: /tmp/bad.s:2: duplicate symbol\n',status=1)
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/asparse /tmp/frontend.s',terminal=1,stdout=expected)
print(f'PASS: {ram} native assembler first-pass operands, symbols/directives, error status and unprivileged use')

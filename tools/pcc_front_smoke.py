"""Native cpp -> retained PCC frontend -> checked shared mip semantic records."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
expected=Path('build/fixtures-pcc/expected').read_text()
artifacts={'/tmp/pccfront':'pcc-front-test.elf','/tmp/types.c':'fixtures-pcc/types.c','/tmp/expected':'fixtures-pcc/expected'}
with private_guest(qemu,iso,ram,'pcc-front',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/lib/cpp /tmp/types.c /tmp/types.i')
    guest.command('/tmp/pccfront /tmp/types.i > /tmp/trace')
    guest.command('cmp /tmp/trace /tmp/expected')
    guest.command("echo 'int broken = 1/0;' > /tmp/bad.i")
    guest.command('/tmp/pccfront /tmp/bad.i > /tmp/badtrace 2> /tmp/error',status=1)
    guest.command("grep -c 'division by zero' /tmp/error",stdout='1\n')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/lib/cpp /tmp/types.c /tmp/bob.i',terminal=1)
    guest.command('/tmp/pccfront /tmp/bob.i > /tmp/bobtrace',terminal=1)
    guest.command('cmp /tmp/bobtrace /tmp/expected',terminal=1)
print(f'PASS: {ram} native cpp/PCC frontend, exact shared IR/layout/semantic records, diagnostics and ordinary-user compilation')

"""Native cpp -> retained PCC frontend -> checked shared mip semantic records."""
from pathlib import Path
import sys
import json
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
expected=Path('build/fixtures-pcc/expected').read_text()
headers=json.loads(Path('build/fixtures-pcc/headers.json').read_text())
artifacts={'/tmp/native.h':'fixtures-pcc/native.h',
    '/tmp/native.c':'fixtures-pcc/native.c','/tmp/native.exp':'fixtures-pcc/native.exp',
    '/tmp/real.c':'fixtures-pcc/real.c','/tmp/real.exp':'fixtures-pcc/real.exp',
    '/tmp/pccfront':'pcc-front-test.elf','/tmp/types.c':'fixtures-pcc/types.c','/tmp/expected':'fixtures-pcc/expected'}
for i,name in enumerate(headers):
    artifacts[f'/tmp/h{i}.c']='fixtures-pcc/'+name+'.c'
    artifacts[f'/tmp/h{i}.exp']='fixtures-pcc/'+name+'.exp'
with private_guest(qemu,iso,ram,'pcc-front',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/lib/cpp /tmp/types.c /tmp/types.i')
    guest.command('/tmp/pccfront /tmp/types.i > /tmp/trace')
    guest.command('cmp /tmp/trace /tmp/expected')
    guest.command("echo 'int broken = 1/0;' > /tmp/bad.i")
    guest.command('/tmp/pccfront /tmp/bad.i > /tmp/badtrace 2> /tmp/error',status=1)
    guest.command("grep -c 'division by zero' /tmp/error",stdout='1\n')
    for name in ['native','real']+[f'h{i}' for i in range(len(headers))]:
        guest.command(f'/lib/cpp /tmp/{name}.c /tmp/header.i')
        guest.command('/tmp/pccfront --native-source /tmp/header.i > /tmp/htrace')
        guest.command(f'cmp /tmp/htrace /tmp/{name}.exp')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/lib/cpp /tmp/types.c /tmp/bob.i',terminal=1)
    guest.command('/tmp/pccfront /tmp/bob.i > /tmp/bobtrace',terminal=1)
    guest.command('cmp /tmp/bobtrace /tmp/expected',terminal=1)
    for name in ['native','real']+[f'h{i}' for i in range(len(headers))]:
        guest.command(f'/lib/cpp /tmp/{name}.c /tmp/bheader.i',terminal=1)
        guest.command('/tmp/pccfront --native-source /tmp/bheader.i > /tmp/bhtrace',terminal=1)
        guest.command(f'cmp /tmp/bhtrace /tmp/{name}.exp',terminal=1)
print(f'PASS: {ram} native cpp/PCC frontend, exact shared IR/layout/semantic records, diagnostics and ordinary-user compilation')

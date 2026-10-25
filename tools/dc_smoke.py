"""Installed dc acceptance on disposable native guests."""
from pathlib import Path
import sys
sys.path.append(str(Path(__file__).resolve().parents[1]/'tests'))
from dc import cases
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={}
examples=list(cases())
for i,(source,_) in enumerate(examples):
    artifact=f'dc-input-{i}'
    Path('build',artifact).write_text(source)
    artifacts[f'/tmp/dc{i}']=artifact
Path('build/dc-bad').write_text('[unterminated')
artifacts['/tmp/dcbad']='dc-bad'
with private_guest(qemu,iso,ram,'dc',artifacts,single_user=False) as guest:
    login(guest,'root','nexix')
    for i,(_,output) in enumerate(examples):
        command=f'dc </tmp/dc{i}'
        if output and not output.endswith('\n'):
            command+='; echo'
            output+='\n'
        guest.command(command,stdout=output)
    guest.command('dc /tmp/dc0 </tmp/dc1',stdout='5\n-3\n')
    guest.command('dc /tmp/absent',stdout="can't open file /tmp/absent\n",status=1)
    guest.command('dc </tmp/dcbad',stdout='dc: unterminated string\n',status=1)
    guest.command("echo '999999999999999999 888888888888888888 *p' | dc",stdout='888888888888888887111111111111111112\n')
print(f'PASS: {ram} installed dc, arbitrary precision, fractions/roots/bases, register/array/macro and errors')

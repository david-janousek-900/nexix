"""Installed V7 bc/dc and recovered lib.b, plus native yacc reproduction."""
from pathlib import Path
import sys
sys.path.append(str(Path(__file__).resolve().parents[1]/'tests'))
from bc import CASES,MATH,MATH_VALUES
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={}
for i,(source,_) in enumerate(CASES):
    artifact=f'bc-input-{i}';Path('build',artifact).write_text(source);artifacts[f'/tmp/bc{i}']=artifact
Path('build/bc-math').write_text(MATH);artifacts['/tmp/bcmath']='bc-math'
Path('build/bc-grammar.y').write_bytes(Path('user/bc/bc.y').read_bytes())
artifacts.update({'/tmp/bc.y':'bc-grammar.y','/tmp/bcexpected.c':'bc/bc.c'})
Path('build/bc-invalid').write_text('2+*3\n');artifacts['/tmp/bcbad']='bc-invalid'
with private_guest(qemu,iso,ram,'bc',artifacts,single_user=False) as guest:
    login(guest,'root','nexix')
    for i,(_,output) in enumerate(CASES):
        command=f'bc </tmp/bc{i}'
        if output and not output.endswith('\n'):command+='; echo';output+='\n'
        guest.command(command,stdout=output)
    # Compare fixed decimal output to the independently checked host oracle bytes.
    import subprocess
    from v7_command import Command
    dc=Command('dc',flags=('-fsigned-char',))
    compiler=Command('bc',sources=['build/bc/bc.c'],flags=(f'-DBC_DC="{dc.binary}"',f'-DBC_LIBRARY="{Path("reference/supplemental/v6/lib.b").resolve()}"'))
    expected=compiler.run(['-l'],data=MATH).stdout.decode()
    assert len(expected.splitlines())==6
    guest.command('bc -l </tmp/bcmath',stdout=expected)
    guest.command('bc </tmp/bcbad',stdout='bc: syntax error on line 1, standard input\n',status=1)
    guest.command('bc /tmp/absent',stdout='bc: cannot open input file on line 1, /tmp/absent\n',status=1)
    guest.command('bc -c </tmp/bc0 >/tmp/dcprog')
    guest.command('dc </tmp/dcprog',stdout='5\n')
    guest.command('cd /tmp')
    guest.command('yacc bc.y',stdout='\nconflicts: 12 shift/reduce, 30 reduce/reduce\n')
    guest.command('cmp y.tab.c bcexpected.c')
print(f'PASS: {ram} installed bc/dc, all six math functions, recursive/control/array paths and native yacc byte reproduction')

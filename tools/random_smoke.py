"""Installed random plus a fixed-clock build of the exact same source."""
from pathlib import Path
import sys
sys.path.append(str(Path(__file__).resolve().parents[1]/'tests'))
# Load the numeric oracle without changing the production command clock.
import importlib.util
spec=importlib.util.spec_from_file_location('random_acceptance',Path('tests/random_command.py'))
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
data=b''.join(f'line{i}\n'.encode() for i in range(30));Path('build/random-input').write_bytes(data)
longline=b'x'*2048+b'\n';Path('build/random-long').write_bytes(longline)
artifacts={'/tmp/rfixed':'random-fixed.elf','/tmp/rinput':'random-input','/tmp/rlong':'random-long'}
with private_guest(qemu,iso,ram,'random',artifacts,single_user=False) as g:
    login(g,'root','nexix')
    g.command('random 1 </tmp/rinput',stdout=data.decode())
    g.command('random 0 </tmp/rinput',stdout=data.decode())
    g.command('random -r 1 </tmp/rlong >/tmp/rout');g.command('cmp /tmp/rout /tmp/rlong')
    g.command('random -e 1')
    for divisor in [2,3,7,256]:
        out,status=oracle.expected(data,divisor)
        g.command(f'/tmp/rfixed {divisor} </tmp/rinput',stdout=out.decode(),status=status)
        _,status=oracle.expected(b'',divisor,True)
        g.command(f'/tmp/rfixed -e {divisor}',status=status)
    g.command('random nan',stdout='random: invalid divisor\n',status=1)
print(f'PASS: {ram} installed random and fixed-clock native source, exact selection/exit sequence and long-line/error paths')

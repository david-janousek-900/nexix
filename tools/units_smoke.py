"""Installed V7 units, pristine database and independently known conversions."""
from pathlib import Path
import re
import sys
sys.path.append(str(Path(__file__).resolve().parents[1]/'tests'))
from units import CASES,values
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/units-input').write_text(''.join(have+'\n'+want+'\n' for have,want,_ in CASES)+'m\nsec\nnosuchunit\n')
artifacts={'/tmp/uinput':'units-input','/tmp/udb':'units-data'}
with private_guest(qemu,iso,ram,'units',artifacts,single_user=False) as g:
    login(g,'root','nexix')
    g.command('cmp /usr/lib/units /tmp/udb')
    g.command('units </tmp/uinput >/tmp/uout')
    token='__UNITS_END__';command='cat /tmp/uout; echo '+token
    g.send(command+'\n');g.expect(re.escape(command.encode()+b'\r\n'))
    output,_=g.expect(rb'(?m)^'+token.encode()+rb'\r?\n');g.expect(rb'root@[^\r\n]*# ')
    actual=values(output)
    assert len(actual)==len(CASES),(actual,output)
    for value,(_,_,want) in zip(actual,CASES):assert abs(value-want)<max(1e-9,abs(want)*1e-6),(value,want)
    assert b'437 units; 3191 bytes' in output and b'conformability' in output and b'cannot recognize nosuchunit' in output,output
    g.command('units /dev/mem',stdout='units: read error\n',status=1)
    g.command('units /tmp/absent',stdout='no table\n',status=1)
print(f'PASS: {ram} installed units, unchanged 437-unit database, independent conversions and conformability/errors')

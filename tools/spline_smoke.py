"""Installed retained spline checked numerically against independent dense systems."""
from pathlib import Path
import re
import sys
sys.path.append(str(Path(__file__).resolve().parents[1]/'tests'))
from spline import CASES,derivative,expected
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={}
for i,(_,points,_,_) in enumerate(CASES):
    artifact=f'spline-input-{i}'
    Path('build',artifact).write_text(''.join(f'{x} {y}\n' for x,y in points))
    artifacts[f'/tmp/si{i}']=artifact
Path('build/spline-bad').write_text('0 1\n2\n');artifacts['/tmp/sbad']='spline-bad'
with private_guest(qemu,iso,ram,'spline',artifacts,single_user=False) as g:
    login(g,'root','nexix')
    for i,(flags,points,k,periodic) in enumerate(CASES):
        g.command('spline '+' '.join([*flags,'-n','24'])+f' </tmp/si{i} >/tmp/sout')
        token=f'__SPLINE_END_{i}__';command='cat /tmp/sout; echo '+token
        g.send(command+'\n');g.expect(re.escape(command.encode()+b'\r\n'))
        output,_=g.expect(rb'(?m)^'+token.encode()+rb'\r?\n');g.expect(rb'root@[^\r\n]*# ')
        rows=[tuple(map(float,line.split())) for line in output.decode().splitlines()]
        count=1+sum(max(1,int(1.001*24*(points[j+1][0]-points[j][0])/(points[-1][0]-points[0][0]))) for j in range(len(points)-1))
        assert len(rows)==count,(i,len(rows),count)
        assert all(rows[j][0]>rows[j+1][0] for j in range(len(rows)-1))
        assert abs(rows[0][0]-points[-1][0])<2e-6 and abs(rows[-1][0]-points[0][0])<2e-6
        curvature=derivative(points,k,periodic)
        for x,y in rows:
            want=expected(points,curvature,x)
            assert abs(y-want)<2e-5,(i,x,y,want)
    g.command('spline </tmp/sbad',stdout='spline: missing ordinate\n',status=1)
    g.command('spline -n 0 </tmp/si0',stdout='spline: invalid point count\n',status=1)
print(f'PASS: {ram} installed spline, independent dense-system natural/general/periodic curves and input bounds')

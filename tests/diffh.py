"""diffh: resynchronizing diff for long files; statuses, long lines and errors.

Short-line inputs are compared with the V7 source built for the host."""
import random
import subprocess
from v7_command import Command
c=Command('diffh')
src=open('reference/v7/usr/src/cmd/diffh.c').read().replace('	char *malloc();\n','')
(c.dir/'ref.c').write_text('#include <stdlib.h>\n'+src)
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char',str(c.dir/'ref.c'),'-o',str(c.dir/'ref')],check=True)
lines=[f'l{i}\n' for i in range(40)]
f1=c.file('f1',''.join(lines))
f2=c.file('f2',''.join(lines[:10]+['new\n']+lines[11:30]+lines[32:]))
c.check([f1,f2],'11c11\n< l10\n---\n> new\n31,32d30\n< l30\n< l31\n',1)
c.check([f1,f1],'',0)
c.check(['-',f2],'11c11\n< l10\n---\n> new\n31,32d30\n< l30\n< l31\n',1,data=f1.read_bytes())
c.check(['-b',c.file('b1','a  b\n'),c.file('b2','a b\n')],'',0)
long='Q'*5000
c.check([c.file('l1',f'x\n{long}1\ny\n'),c.file('l2',f'x\n{long}2\ny\n')],f'2,$c2,$\n< {long}1\n< y\n---\n> {long}2\n> y\n',1)
c.check([c.file('u1','a\nb'),c.file('u2','a\nb\n')],'',0)
d=c.dir/'d';d.mkdir();(d/'f1').write_text(''.join(lines))
c.check([d,f1],'',0)
c.check([f1],'',2,stderr='diffh: must have 2 file arguments\n')
c.check(['-','-'],'',2,stderr="diffh: can't do - -\n")
c.check([f1,c.dir/'missing'],'',2,stderr=f"diffh: can't access {c.dir/'missing'}\n")
far=c.file('far',''.join(f'q{i}\n' for i in range(100)))
r=c.run([f1,far]);assert r.returncode==2 and r.stderr==b"diffh: can't resynchronize\n",r
rng=random.Random(6)
for trial in range(300):
    base=[f'{rng.randrange(8)}\n' for _ in range(rng.randrange(1,60))]
    other=list(base)
    for _ in range(rng.randrange(0,4)):
        k=rng.randrange(len(other)+1)
        if rng.random()<0.5: other.insert(k,'x\n')
        elif other: del other[min(k,len(other)-1)]
    a=c.file('ra',''.join(base));b=c.file('rb',''.join(other))
    want=subprocess.run([str(c.dir/'ref'),str(a),str(b)],capture_output=True)
    r=c.run([a,b])
    assert r.stdout==want.stdout and (r.returncode==2)==(want.returncode==1 and bool(want.stderr)),(r,want)
print('PASS: diffh ASan/UBSan changes, deletions, stdin, -b, 5000-byte/unterminated lines, directories, statuses, errors and 300 reference comparisons')

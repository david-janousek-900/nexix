"""diff3 helper: three-way reports, -e/-3/-x edit scripts, long lines and errors.

Inputs are also compared with the V7 source built for the host, with only its
getline name and the missing return value of duplicate() repaired."""
import random
import subprocess
from v7_command import Command
c=Command('diff3')
diff=Command('diff')
src=open('reference/v7/usr/src/cmd/diff3.c').read().replace('getline(','getln(').replace('				repos(nchar);\n				return;','				repos(nchar);\n				return(0);')
(c.dir/'ref.c').write_text('#include <stdlib.h>\n'+src)
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char',str(c.dir/'ref.c'),'-o',str(c.dir/'ref')],check=True)
def three(t1,t2,t3,*flags,ref=True,status=0):
    f=[c.file(f'f{i}',t) for i,t in enumerate((t1,t2,t3),1)]
    (c.dir/'d13').write_bytes(diff.run([f[0],f[2]]).stdout);(c.dir/'d23').write_bytes(diff.run([f[1],f[2]]).stdout)
    args=[*flags,c.dir/'d13',c.dir/'d23',*f]
    r=c.run(args);assert r.returncode==status,r
    if ref:
        want=subprocess.run([str(c.dir/'ref'),*map(str,args)],capture_output=True)
        assert r.stdout==want.stdout,(args,r.stdout,want.stdout)
    return r.stdout.decode()
base='a\nb\nc\nd\n'
assert three(base,'a\nB\nc\nd\n','a\nb\nc\nD\n')=='====2\n1:2c\n2:2c\n  B\n3:2c\n  b\n====3\n1:4c\n2:4c\n  d\n3:4c\n  D\n'
assert three(base,'a\nb\nc\nd\n','a\nX\nc\nd\n')=='====3\n1:2c\n2:2c\n  b\n3:2c\n  X\n'
assert three('a\n1\nc\n','a\n2\nc\n','a\n3\nc\n')=='====\n1:2c\n  1\n2:2c\n  2\n3:2c\n  3\n'
assert three(base,base,base)==''
assert three(base,'a\nB\nc\nd\n','a\nb\nc\nD\n','-e')=='4c\nD\n.\n'
assert three('a\n1\nc\n','a\n2\nc\n','a\n3\nc\n','-e')=='2c\n3\n.\n'
assert three('a\n1\nc\n','a\n2\nc\n','a\n3\nc\n','-x')=='2c\n3\n.\n'
assert three('a\n1\nc\n','a\n2\nc\n','a\n3\nc\n','-3')==''
long='W'*3000
out=three(f'{long}\nx\n',f'{long}\ny\n',f'{long}\nz\n',ref=False);assert out=='====\n1:2c\n  x\n2:2c\n  y\n3:2c\n  z\n',out
many=''.join(f'{i}\n' for i in range(1200))
out=three(many,many,many.replace('0\n','X\n'),ref=False);assert out.count('====3')==120,out[:100]
c.check([c.dir/'d13'],'',1,stderr='diff3: arg count\n')
c.check([c.dir/'missing',c.dir/'d23',c.dir/'f1',c.dir/'f2',c.dir/'f3'],f"diff3: can't open {c.dir/'missing'}\n",1)
rng=random.Random(12)
for trial in range(300):
    b=[rng.choice('abcd') for _ in range(rng.randrange(1,15))]
    def vary():
        o=list(b)
        for _ in range(rng.randrange(0,3)):
            k=rng.randrange(len(o)+1)
            if rng.random()<0.5:o.insert(k,rng.choice('xyz'))
            elif o:o[min(k,len(o)-1)]=rng.choice('xyz')
        return ''.join(x+'\n' for x in o)
    t1,t2,t3=vary(),vary(),vary()
    for flags in ([],['-e'],['-x'],['-3']):
        three(t1,t2,t3,*flags)
print('PASS: diff3 ASan/UBSan three-way reports, -e/-x/-3 scripts, 3000-byte lines, 120 changes, errors and 1,200 reference comparisons')

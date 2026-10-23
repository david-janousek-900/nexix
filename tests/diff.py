"""diff: Stone LCS output in normal/-e/-f/-b forms, stdin, directories, limits and errors.

Newline-terminated inputs are compared with the V7 source built for the host
with its use-after-free "compacting" reallocation replaced by realloc."""
import random
import subprocess
from v7_command import Command
c=Command('diff')
src=open('reference/v7/usr/src/cmd/diff.c').read()
src=src.replace('	free(p);\n	free(dummy);\n	dummy = malloc(1);\n','').replace('	extern char *malloc();\n','').replace('	char *realloc();\n','').replace('FILE *fopen();\n','').replace('char *mktemp();\n','')
(c.dir/'ref.c').write_text('#include <stdlib.h>\n#include <unistd.h>\n'+src)
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char',str(c.dir/'ref.c'),'-o',str(c.dir/'ref')],check=True,capture_output=True)
f1=c.file('f1','a\nb\nc\nd\n');f2=c.file('f2','a\nx\nc\nd\ne\n')
c.check([f1,f2],'2c2\n< b\n---\n> x\n4a5\n> e\n',1)
c.check(['-e',f1,f2],'4a\ne\n.\n2c\nx\n.\n',1)
c.check(['-f',f1,f2],'c2\nx\n.\na4\ne\n.\n',1)
c.check([f1,f1],'',0)
c.check(['-',f2],'2c2\n< b\n---\n> x\n4a5\n> e\n',1,data=f1.read_bytes())
c.check([c.file('e',''),f1],'0a1,4\n> a\n> b\n> c\n> d\n',1)
c.check([f1,c.file('e2','')],'1,4d0\n< a\n< b\n< c\n< d\n',1)
c.check(['-b',c.file('s1','a  b\nc\n'),c.file('s2','a b \nc\n')],'',0)
c.check([c.file('s1','a  b\nc\n'),c.file('s2','a b \nc\n')],'1c1\n< a  b\n---\n> a b \n',1)
d=c.dir/'dir';d.mkdir();(d/'f1').write_text('a\nB\nc\nd\n')
c.check([d,f1],'2c2\n< B\n---\n> b\n',1)
c.check([f1,d],'2c2\n< b\n---\n> B\n',1)
c.check([c.file('u1','a\nb'),c.file('u2','a\nc')],'2c2\n< b\n---\n> c\n',1)
c.check([c.file('u3','a\nb'),c.file('u4','a\nb\n')],'',0)
long='L'*20000
c.check([c.file('l1',f'{long}1\nsame\n'),c.file('l2',f'{long}2\nsame\n')],f'1c1\n< {long}1\n---\n> {long}2\n',1)
big=''.join(f'line {i}\n' for i in range(20000))
c.check([c.file('b1',big),c.file('b2',big.replace('line 777\n','changed\n'))],'778c778\n< line 777\n---\n> changed\n',1)
c.check([f1],'',2,stderr='diff: arg count\n')
c.check([f1,c.dir/'missing'],'',2,stderr=f"diff: cannot open {c.dir/'missing'}\n")
rng=random.Random(2)
for trial in range(400):
    base=[rng.choice('abcde ') for _ in range(rng.randrange(0,25))]
    other=list(base)
    for _ in range(rng.randrange(0,6)):
        op=rng.randrange(3);k=rng.randrange(len(other)+1)
        if op==0: other.insert(k,rng.choice('abcxy '))
        elif other and op==1: del other[min(k,len(other)-1)]
        elif other: other[min(k,len(other)-1)]=rng.choice('xyz')
    a=c.file('ra',''.join(x+'\n' for x in base));b=c.file('rb',''.join(x+'\n' for x in other))
    for flags in ([],['-e'],['-f'],['-b']):
        want=subprocess.run([str(c.dir/'ref'),*flags,str(a),str(b)],capture_output=True)
        c.check(flags+[a,b],want.stdout,want.returncode)
print('PASS: diff ASan/UBSan normal/-e/-f/-b output, stdin, directories, empty/unterminated/20 KB lines, 20000-line files, errors and 1,600 reference comparisons')

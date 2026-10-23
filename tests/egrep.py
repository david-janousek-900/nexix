"""egrep: generated V7 DFA, flags, high bytes, long/unterminated lines and errors.

ASCII short-line inputs are compared with the V7 source (only its old
initializers and pointer-typed declarations repaired) built for the host."""
import os
import random
import subprocess
from v7_command import Command
root=os.getcwd()
yacc=os.path.join(root,'build/host-nx-yacc')
subprocess.run(['make','-s','build/host-nx-yacc'],check=True)
env=dict(os.environ,NX_YACCPAR=os.path.join(root,'rootfs/usr/lib/yaccpar'))
def generate(directory,source):
    (directory/'egrep.y').write_text(source)
    subprocess.run([yacc,'egrep.y'],cwd=directory,env=env,check=True,capture_output=True)
    return directory/'y.tab.c'
import tempfile,pathlib
gen=pathlib.Path(tempfile.mkdtemp())
native=generate(gen,open('user/v7/cmd/egrep.y').read())
c=Command('egrep',sources=[native])
refdir=gen/'ref';refdir.mkdir()
src=open('reference/v7/usr/src/cmd/egrep.y').read()
for a,b in (('int line 1;','int line = 1;'),('int nxtchar 0;','int nxtchar = 0;'),('yyerror(s) {','yyerror(s) char *s; {'),('int	fname;','char	*fname;')):
    src=src.replace(a,b)
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char','-include','stdlib.h','-include','unistd.h',str(generate(refdir,src)),'-o',str(refdir/'egrep')],check=True)
text='apple pie\nbanana split\ncherry tart\napple crumble\nplain\n'
f=c.file('text',text)
c.check(['apple|tart',f],'apple pie\ncherry tart\napple crumble\n')
c.check(['^(apple|plain)',f],'apple pie\napple crumble\nplain\n')
c.check(['e$',f],'apple pie\napple crumble\n')
c.check(['p+l[ae]',f],'apple pie\napple crumble\nplain\n')
c.check(['-v','a',f],'',1)
c.check(['-c','[^a-z ]',f],'0\n',1)
c.check(['-n','an(an)*a',f],'2:banana split\n')
c.check(['(x|y)?pl.in',f],'plain\n')
c.check(['-l','pie',f,f],f'{f}\n{f}\n')
c.check(['-h','pie',f,f],'apple pie\napple pie\n')
c.check(['pie',f,f],f'{f}:apple pie\n{f}:apple pie\n')
p=c.file('pats','pie\ntart\n')
c.check(['-f',p,f],'apple pie\ncherry tart\n')
c.check(['-f',p],'cherry tart\n',data='cherry tart\nplum\n')
c.check(['-b','plain',c.file('blocks','x'*600+'\nplain\n')],'1:plain\n')
hi=c.file('hi',b'caf\xe9\nplain\n\xff\xfe\n')
c.check([b'[\x80-\xff]',hi],b'caf\xe9\n\xff\xfe\n')
c.check([b'\xe9$',hi],b'caf\xe9\n')
c.check([b'\xff\xfe',hi],b'\xff\xfe\n')
r=c.run(['.',c.file('nul',b'a\0b\n')]);assert r.stdout==b'a\0b\n',r
long='q'*5000+'needle'+'r'*5000+'\n'
c.check(['ne+dle'],long,data='short\n'+long+'tail\n')
c.check(['end$'],'the end',data='x\nthe end')
c.check(['-v','x'],'last',data='x\nlast')
c.check(['[a-'],'',2,stderr='egrep: syntax error\n')
c.check(['(a'],'',2,stderr='egrep: syntax error\n')
c.check(['a',c.dir/'missing'],'',2,stderr=f"egrep: can't open {c.dir/'missing'}\n")
c.check(['-f',c.dir/'missing'],'',2,stderr=f"egrep: can't open {c.dir/'missing'}\n")
c.check([],'',2)
c.check(['('+'|'.join(f'w{i}x' for i in range(120))+')'],'w119x\n',data='w119x\nw120x\n')
c.check(['(a|b)*a(a|b)(a|b)(a|b)(a|b)(a|b)(a|b)(a|b)'],'',1,data='bbbbbbbbbb\n')
c.check(['(a|b)*a'+'(a|b)'*9],'',2,data='b\n',stderr='egrep: regular expression too long\n')
rng=random.Random(5)
atoms=['a','b','.','[ab]','[^a]','(a|b)','x']
for trial in range(300):
    pat=''.join(rng.choice(atoms)+rng.choice(['','*','+','?']) for _ in range(rng.randrange(1,4)))
    if rng.random()<0.3: pat='^'+pat
    if rng.random()<0.3: pat+='$'
    if rng.random()<0.3: pat+='|'+rng.choice(atoms)
    data=''.join(''.join(rng.choice('abx ') for _ in range(rng.randrange(0,10)))+'\n' for _ in range(rng.randrange(1,8)))
    for flags in ([],['-v'],['-c'],['-n']):
        want=subprocess.run([str(refdir/'egrep'),*flags,pat],input=data.encode(),capture_output=True)
        c.check(flags+[pat],want.stdout,want.returncode,data=data)
print('PASS: egrep ASan/UBSan DFA, flags, pattern files, high bytes, NULs, 10 KB/unterminated lines, limits, errors and 1,200 reference comparisons')

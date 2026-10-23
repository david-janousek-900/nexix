"""sort: keys/flags, merges beyond the workspace, long lines, temp files and errors.

Small ASCII inputs are compared with the V7 source built for the host with
signed characters (as on the PDP-11) and its brk workspace replaced by malloc."""
import os
import random
import subprocess
from v7_command import Command
c=Command('sort')
src=open('reference/v7/usr/src/cmd/sort.c').read()
a=src.index('	ep = end + MEM;');b=src.index('	tspace = (char *)(lspace + nlines);')
src=src[:a]+'	lspace = (int *)malloc(MEM*8);\n	nlines = (MEM*8-L)/(5*sizeof(char *));\n	ntext = nlines*8;\n'+src[b:]
src=src.replace('char	*sbrk();\nchar	*brk();','').replace('	extern char end[1];\n	char *ep;\n','').replace('qsort(','sortq(')
(c.dir/'ref.c').write_text('#include <stdlib.h>\n#include <unistd.h>\n#include <string.h>\n'+src)
subprocess.run(['cc','-w','-std=gnu89','-fsigned-char','-fcommon',str(c.dir/'ref.c'),'-o',str(c.dir/'ref')],check=True)
tmp=c.dir/'tmp';tmp.mkdir()
def leftovers(): return list(tmp.iterdir())
c.check([],'a\nb\nb\nc\n',data='b\na\nc\nb\n')
c.check(['-u'],'a\nb\nc\n',data='b\na\nc\nb\n')
c.check(['-r'],'c\nb\nb\na\n',data='b\na\nc\nb\n')
c.check(['-n'],'-3\n9 y\n10 x\n100 z\n',data='10 x\n9 y\n100 z\n-3\n')
c.check(['+1n'],'b 1\nc 2\na 3\n',data='a 3\nb 1\nc 2\n')
c.check(['-t:','+1','-2'],'x:a:9\ny:b:1\n',data='y:b:1\nx:a:9\n')
c.check(['-f'],'a\nB\nc\n',data='c\nB\na\n')
c.check(['-d'],'ab\na-c\n',data='a-c\nab\n')
c.check([],'x\ny\n',data='y\nx')
c.check(['-c'],'',data='a\nb\n')
c.check(['-c'],'',1,data='b\na\n',stderr='sort: disorder:a\n')
c.check(['-cu'],'',1,data='a\na\n',stderr='sort: nonunique:a\n')
f1=c.file('f1','b\nd\n');f2=c.file('f2','a\nc\n')
c.check(['-m',f1,f2],'a\nb\nc\nd\n')
many=[c.file(f'm{i}',''.join(f'{j:03}\n' for j in range(i,300,10))) for i in range(10)]
c.check(['-T',tmp,'-m',*many],''.join(f'{j:03}\n' for j in range(300)));assert not leftovers()
c.check(['-T',tmp,'-mu',*many,*many],''.join(f'{j:03}\n' for j in range(300)))
c.check(['-o',c.dir/'out',f1,f2],'');assert (c.dir/'out').read_text()=='a\nb\nc\nd\n'
c.check(['-m','-o',f1,f1,f2],'');assert f1.read_text()=='a\nb\nc\nd\n'
c.check([],b'a\nz\n\xe9\n',data=b'\xe9\nz\na\n')
c.check([c.dir/'missing'],'',1,stderr=f"sort: can't open {c.dir/'missing'}\n")
c.check(['-T',tmp,'-o',c.dir/'nodir'/'x'],'',1,data='a\n',stderr=f"sort: can't create {c.dir/'nodir'/'x'}\n");assert not leftovers()
c.check(['-T',c.dir/'nodir'],'a\n',data='a\n')
c.check(['+'*1+'1']*11,'',1,data='a\n',stderr='sort: too many keys\n')
rng=random.Random(4)
big=[''.join(rng.choice('abcdefghij') for _ in range(rng.randrange(1,40))) for _ in range(300000)]
data=('\n'.join(big)+'\n').encode()
r=c.run(['-T',tmp],data);assert r.returncode==0 and r.stdout==('\n'.join(sorted(big))+'\n').encode(),r.stderr;assert not leftovers()
r=c.run(['-T',tmp,'-u','-r'],data);assert r.stdout==('\n'.join(sorted(set(big),reverse=True))+'\n').encode();assert not leftovers()
long=[rng.choice('xyz')*rng.randrange(1000,200000) for _ in range(30)]
r=c.run(['-T',tmp],('\n'.join(long)+'\n').encode());assert r.stdout==('\n'.join(sorted(long))+'\n').encode();assert not leftovers()
r=c.run(['-T',tmp],'q'*600000+'\n');assert r.returncode==1 and r.stderr==b'sort: line too long\n';assert not leftovers()
for trial in range(300):
    data=''.join(' '.join(''.join(rng.choice('ab1 -.\t') for _ in range(rng.randrange(0,5))) for _ in range(rng.randrange(1,4)))+'\n' for _ in range(rng.randrange(1,30)))
    flags=rng.choice([[],['-n'],['-r'],['-u'],['-f'],['-d'],['-b'],['+1'],['+1n'],['-t.','+1','-2'],['+0.1','-0.3'],['-nr'],['+1','-2','+0r'],['-u','+1'],['-i'],['-c']])
    want=subprocess.run([str(c.dir/'ref'),*flags],input=data.encode(),capture_output=True)
    c.check(['-T',tmp,*flags],want.stdout,want.returncode,data=data)
assert not leftovers()
print('PASS: sort ASan/UBSan keys and flags, -c/-m/-o, 8-bit order, multi-level merges, 200 KB lines, workspace and temp-file limits, errors and 300 reference comparisons')

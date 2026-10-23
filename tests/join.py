"""join: keys, repeats, -a/-e/-o/-t/-j, long and unterminated lines, bounds and errors."""
from v7_command import Command
c=Command('join')
a=c.file('a','a 1\nb 2\nb 3\nd 4\n');b=c.file('b','b x\nb y\nc z\nd w\n')
both='b 2 x\nb 2 y\nb 3 x\nb 3 y\nd 4 w\n'
c.check([a,b],both)
c.check(['-a1',a,b],'a 1\n'+both)
c.check(['-a2',a,b],'b 2 x\nb 2 y\nb 3 x\nb 3 y\nc z\nd 4 w\n')
c.check(['-a',a,b],'a 1\nb 2 x\nb 2 y\nb 3 x\nb 3 y\nc z\nd 4 w\n')
c.check(['-o','2.2','1.1',a,b],'x b\ny b\nx b\ny b\nw d\n')
c.check(['-a1','-e','NONE','-o','1.1','2.2',a,b],'a NONE\nb x\nb y\nb x\nb y\nd w\n')
c.check(['-o','1.9','1.1',a,b],' b\n b\n b\n b\n d\n')
t1=c.file('t1','k:1:\nm:2:z\n');t2=c.file('t2','k:A\nm:B\n')
c.check(['-t:',t1,t2],'k:1:A\nm:2:z:B\n')
c.check(['-j1','2','-j2','1',c.file('j1','x a\ny b\n'),c.file('j2','a P\nb Q\n')],'a x P\nb y Q\n')
c.check(['-',b],both,data=a.read_bytes())
many=' '.join(f'f{i}' for i in range(60))
c.check([c.file('m1',f'k {many}\n'),c.file('m2','k end\n')],f'k {many} end\n')
long='L'*9000
c.check([c.file('l1',f'{long} 1\n'),c.file('l2',f'{long} 2')],f'{long} 1 2\n')
c.check([c.file('s1','k\n'),c.file('s2','k v\n')],'k v\n')
c.check(['-j','2',c.file('s3','only\n'),c.file('s4','x\n')],' only x\n')
usage='join: usage: join [-j1 x -j2 y] [-o list] file1 file2\n'
c.check([a],'',1,stderr=usage)
c.check(['-j','0',a,b],'',1,stderr=usage)
c.check(['-e'],'',1,stderr=usage)
c.check(['-o'],'',1,stderr=usage)
c.check([a,c.dir/'missing'],'',1,stderr=f"join: can't open {c.dir/'missing'}\n")
print('PASS: join ASan/UBSan keys, repeats, -a/-e/-o/-t/-j, 60 fields, 9000-byte/unterminated lines and errors')

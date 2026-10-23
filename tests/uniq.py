"""uniq: -u/-d/-c, field and character skips, long lines, files and errors."""
from v7_command import Command
c=Command('uniq')
data='a\na\nb\nc\nc\nc\nd\n'
c.check([],'a\nb\nc\nd\n',data=data)
c.check(['-u'],'b\nd\n',data=data)
c.check(['-d'],'a\nc\n',data=data)
c.check(['-c'],'   2 a\n   1 b\n   3 c\n   1 d\n',data=data)
c.check(['-1'],'x a\nx b\n',data='x a\ny a\nx b\n')
c.check(['+2'],'aaa\nabb\n',data='aaa\nbba\nabb\n')
c.check(['-1','+2'],'1 xb\n',data='1 xb\n2 yb\n')
c.check([],'z\nq\n',data='z\nz\nq')
long='w'*4000
c.check([],f'{long}\n{long}x\n',data=f'{long}\n{long}\n{long}x\n')
c.check([],'',data='')
i=c.file('in','p\np\n');o=c.dir/'out'
c.check([i,o],'')
assert o.read_text()=='p\n'
c.check([c.dir/'missing'],'',1,stderr=f"cannot open {c.dir/'missing'}\n")
c.check([i,c.dir/'nodir'/'o'],'',1,stderr=f"cannot create {c.dir/'nodir'/'o'}\n")
c.check(['-99999999'],'',1,data='a\n')
print('PASS: uniq ASan/UBSan modes, field/char skips, 4000-byte lines, files and errors')

"""split: chunk sizes, names, byte reconstruction and limits."""
from v7_command import Command
c=Command('split')
lines=''.join(f'{i}\n' for i in range(2500))
c.check([],data=lines)
assert [p.name for p in sorted(c.dir.glob('xa?'))]==['xaa','xab','xac']
assert ''.join((c.dir/n).read_text() for n in ('xaa','xab','xac'))==lines
assert (c.dir/'xaa').read_text().count('\n')==1000
f=c.file('in','a\nb\nc\nd\ne')
c.check(['-2',f,'part'])
assert [(c.dir/n).read_text() for n in ('partaa','partab','partac')]==['a\nb\n','c\nd\n','e']
c.check(['-1','-','pipe'],data='p\nq\n')
assert (c.dir/'pipeab').read_text()=='q\n'
c.check(['-1',c.dir/'missing'],'',1)
c.check(['-0'],'',1,stderr='usage: split [-n] [file [name]]\n')
c.check(['-z'],'',1)
c.check(['-99999999999'],'',1)
c.check(['-1','-','y'*98],'',1,data='a\n',stderr='split: name too long\n')
c.check(['-1','-','many'],'',1,data='x\n'*677,stderr='split: too many output files\n')
assert (c.dir/'manyzz').read_text()=='x\n'
print('PASS: split ASan/UBSan counts, names, reconstruction, 676-file and name limits')

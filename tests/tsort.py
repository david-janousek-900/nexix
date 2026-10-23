"""tsort: ordering, duplicates, cycles, odd data, long names and errors."""
from v7_command import Command
c=Command('tsort')
c.check([],'a\nb\nc\nd\n',data='a b\nb c\nc d\n')
c.check([],'a\nb\nc\nd\n',data='c d b c\n a b')
c.check([],'x\ny\nz\n',data='x x\ny y\nz z\nx y\n')
r=c.check([],'a\nb\nc\nd\n',data='a b b c c b c d\n',stderr='tsort: cycle in reverse order\ntsort: b\ntsort: c\n')
c.check([],'',1,data='a b c\n',stderr='tsort: odd data\n')
c.check([],'',data='')
long='n'*500
c.check([],f'{long}\n{long}x\n',data=f'{long} {long}x\n')
f=c.file('graph','one two\n')
c.check([f],'one\ntwo\n')
c.check([c.dir/'missing'],'',1,stderr=f"tsort: cannot open {c.dir/'missing'}\n")
print('PASS: tsort ASan/UBSan order, duplicates, cycle report, odd data, 500-byte names and errors')

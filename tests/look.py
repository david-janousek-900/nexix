"""look: binary search, -d/-f/-t, default folding, long entries and errors."""
from v7_command import Command
c=Command('look')
words=[f'w{i:05}' for i in range(3000)]
d=c.file('d','\n'.join(words)+'\n')
c.check(['w0299',d],''.join(f'w0299{i}\n' for i in range(10)))
c.check(['w02999',d],'w02999\n')
c.check(['w0000',d],''.join(f'w0000{i}\n' for i in range(10)))
c.check(['w00000',d],'w00000\n')
c.check(['x',d],'')
c.check(['a',d],'')
f=c.file('f','Apple\napricot\nBanana\nban-dit\nbanjo\n')
c.check(['-df','BAN',f],'Banana\nban-dit\nbanjo\n')
c.check(['-f','ap',f],'Apple\napricot\n')
t=c.file('t','alpha:1\nbeta:2\nbeta:3\ngamma:4\n')
c.check(['-t:','beta',t],'beta:2\nbeta:3\n')
long='q'*3000
c.check(['q'*400,c.file('l',f'a\n{long}\nz\n')],f'{long}\n')
c.check(['word',c.dir/'missing'],'',2,stderr=f"look: can't open {c.dir/'missing'}\n")
c.check([],'')
print('PASS: look ASan/UBSan search boundaries, -d/-f/-t, 3000-byte entries and errors')

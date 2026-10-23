"""tee: copies, append, many outputs, open/write failures."""
import os
from v7_command import Command
c=Command('tee')
data=bytes(range(256))*40
r=c.check([c.dir/'one',c.dir/'two'],data,data=data)
assert (c.dir/'one').read_bytes()==data==(c.dir/'two').read_bytes()
c.file('app','old\n')
c.check(['-a',c.dir/'app'],'new\n',data='new\n')
assert (c.dir/'app').read_bytes()==b'old\nnew\n'
c.check([c.dir/'app'],'x\n',data='x\n')
assert (c.dir/'app').read_bytes()==b'x\n'
names=[c.dir/f'f{i}' for i in range(40)]
c.check(names,'many\n',data='many\n')
assert all(p.read_bytes()==b'many\n' for p in names)
c.check([c.dir/'nodir'/'x',c.dir/'ok'],'y\n',1,data='y\n',stderr=f"tee: cannot open {c.dir/'nodir'/'x'}\n")
assert (c.dir/'ok').read_bytes()==b'y\n'
if os.path.exists('/dev/full'):
    c.check(['/dev/full',c.dir/'after'],'z\n',1,data='z\n')
    assert (c.dir/'after').read_bytes()==b'z\n'
print('PASS: tee ASan/UBSan copies, append, 40 outputs, open and write failures')

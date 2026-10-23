"""rev: line reversal, unbounded lines, unterminated input and errors."""
from v7_command import Command
c=Command('rev')
c.check([],'cba\n\nfed\n',data='abc\n\ndef\n')
c.check([],'zyx\n',data='xyz')
long=bytes(range(32,127))*50
c.check([],long[::-1]+b'\n',data=long+b'\n')
a=c.file('a','12\n');b=c.file('b','34\n')
c.check([a,b],'21\n43\n')
c.check([c.dir/'missing'],'',1,stderr=f"rev: cannot open {c.dir/'missing'}\n")
print('PASS: rev ASan/UBSan reversal, long and unterminated lines, files and errors')

"""fgrep: Aho-Corasick matching, flags, long/unterminated lines, NULs and errors.

Newline-terminated short-line inputs are compared with the unmodified V7
source built for the host."""
import random
import subprocess
from v7_command import Command
c=Command('fgrep')
ref=c.dir/'ref'
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char','-include','stdlib.h','-include','unistd.h','reference/v7/usr/src/cmd/fgrep.c','-o',str(ref)],check=True)
text='apple pie\nbanana split\ncherry tart\napple crumble\nplain\n'
f=c.file('text',text)
c.check(['apple',f],'apple pie\napple crumble\n')
c.check(['-e','-v',f],'',1)
c.check(['-v','apple',f],'banana split\ncherry tart\nplain\n')
c.check(['-c','apple',f],'2\n')
c.check(['-n','tart',f],'3:cherry tart\n')
c.check(['-x','plain',f],'plain\n')
c.check(['-x','-v','plain',f],'apple pie\nbanana split\ncherry tart\napple crumble\n')
c.check(['-l','tart',f,f],f'{f}\n{f}\n')
c.check(['split',f,f],f'{f}:banana split\n{f}:banana split\n')
c.check(['-h','split',f,f],'banana split\nbanana split\n')
c.check(['-s','split',f],'')
c.check(['pie\ntart',f],'apple pie\ncherry tart\n')
w=c.file('words','pie\nsplit\n')
c.check(['-f',w,f],'apple pie\nbanana split\n')
c.check(['-b','plain',c.file('blocks','x'*600+'\nplain\n')],'1:plain\n')
c.check(['zz'],'',1,data='aa\n')
long='q'*5000+'needle'+'r'*5000+'\n'
c.check(['needle'],long,data='short\n'+long+'tail\n')
c.check(['end'],'the end',data='x\nthe end')
c.check(['-v','x'],'last',data='x\nlast')
c.check(['b'],b'a\0b\n',data=b'a\0b\n')
c.check(['-c','\n'],'1\n',data='a\n')
c.check(['x',c.dir/'missing'],'',2,stderr=f"fgrep: can't open {c.dir/'missing'}\n")
c.check(['-f',c.dir/'missing'],'',2,stderr=f"fgrep: can't open {c.dir/'missing'}\n")
c.check(['-q','x'],'x\n',data='x\n',stderr='fgrep: unknown flag\n')
c.check([],'',2)
big=c.file('big',''.join(f'word{i}\n' for i in range(5000)))
c.check(['-f',big],'word4999\n',data='word4999\n')
c.check(['-c','-f',big],'3\n',data='word1\nnope\nword12\nword333\n')
c.check(['-f',c.file('huge',''.join(f'word{i}\n' for i in range(40000)))],'',2,data='x\n',stderr='fgrep: wordlist too large\n')
rng=random.Random(11)
for trial in range(300):
    pats='\n'.join(''.join(rng.choice('ab') for _ in range(rng.randrange(1,4))) for _ in range(rng.randrange(1,4)))
    data=''.join(''.join(rng.choice('ab ') for _ in range(rng.randrange(0,12)))+'\n' for _ in range(rng.randrange(1,8)))
    for flags in ([],['-v'],['-x'],['-c'],['-n'],['-vx']):
        want=subprocess.run([str(ref),*flags,pats],input=data.encode(),capture_output=True)
        c.check(flags+[pats],want.stdout,want.returncode,data=data)
for trial in range(100):
    data=''.join('ab '[rng.randrange(3)]*rng.randrange(0,300)+'\n' for _ in range(rng.randrange(1,12)))
    for flags in (['-b'],['-bv'],['-bn']):
        want=subprocess.run([str(ref),*flags,'b'],input=data.encode(),capture_output=True)
        c.check(flags+['b'],want.stdout,want.returncode,data=data)
print('PASS: fgrep ASan/UBSan flags, word files, 5000 words, 10 KB/unterminated lines, NULs, errors and 2,100 reference comparisons')

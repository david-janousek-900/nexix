"""col: reverse/half line feeds, overstrikes, tabs, SO/SI, -b/-f/-x, long lines.

Short random inputs are compared with the unmodified V7 algorithm built from
reference/v7 (with only its host-incompatible declarations removed)."""
import random
import subprocess
from v7_command import Command
c=Command('col')
ref=c.dir/'ref.c'
src=open('reference/v7/usr/src/cmd/col.c').read()
src=src.replace('char *malloc();','').replace('char *strcpy();','').replace('char fbuff[BUFSIZ];','static char fbuff[BUFSIZ];')
ref.write_text('#include <stdlib.h>\n#include <string.h>\n'+src)
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char',str(ref),'-o',str(c.dir/'ref')],check=True)
ESC=b'\033'
c.check([],'up\n  down\n',data=b'\n  down\x0b\rup\n\n')
c.check([],'one\ntwo\n',data=b'one\ntwo\n')
c.check([],'a\bb\n',data=b'a\bb\n')
c.check(['-b'],'b\n',data=b'a\bb\n')
c.check([],'\tx\n',data=b'        x\n')
c.check(['-x'],'        x\n',data=b'        x\n')
c.check([],'a\bab\n',data=b'a\rab\n')
c.check([],b'\x0ea\x0f\n',data=b'\x0ea\x0f\n')
c.check([],b'x\n',data=b'x\0\n')
long=b'z'*5000
c.check([],long+b'\n',data=long+b'\n')
c.check([],'',2,data=b'\t'*200000+b'x\n')
c.check(['-q'],'',2,stderr='%s: bad option letter q\n'%c.binary)
c.check(['q'],'',2,stderr='%s: bad option q\n'%c.binary)
rng=random.Random(7)
alphabet=[b'a',b'b',b'_',b' ',b'\t',b'\b',b'\r',b'\n',b'\x0b',ESC+b'7',ESC+b'8',ESC+b'9',b'\x0e',b'\x0f']
for trial in range(400):
    data=b''.join(rng.choice(alphabet) for _ in range(rng.randrange(1,60)))
    for flags in ([],['-b'],['-f'],['-x']):
        want=subprocess.run([str(c.dir/'ref'),*flags],input=data,capture_output=True).stdout
        c.check(flags,want,data=data)
print('PASS: col ASan/UBSan line motions, overstrikes, tabs, shift modes, flags, 5000-byte lines and 1600 reference comparisons')

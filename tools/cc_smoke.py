"""Installed V7 cc driver: cpp, c0, c1/fc1, c2, as and ld."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-cc');F.mkdir(parents=True,exist_ok=True)
(F/'main.c').write_text('#include <stdio.h>\n#include "defs.h"\nint twice();\nmain() { double d; d = VAL; printf("cc %d %d %g\\n", VAL, twice(21), d / 4); return(0); }\n')
(F/'twice.c').write_text('twice(n) { return(n * 2); }\n')
(F/'defs.h').write_text('#define DEFAULT 1\n')
(F/'one.s').write_text('.text\n.globl twice\ntwice:\n movl %edi,%eax\n addl %edi,%eax\n ret\n.section .note.GNU-stack,"",@progbits\n')
(F/'bad.c').write_text('main() { x = ; }\n')
art={f'/tmp/{n}':f'fixtures-cc/{n}' for n in ('main.c','twice.c','defs.h','one.s','bad.c')}
OUT='cc 3 42 0.75\n'
def run(g,d,t):
    c=lambda line,**k:g.command(line,terminal=t,**k)
    c(f'mkdir {d}; cd {d}; cp /tmp/main.c /tmp/twice.c /tmp/one.s /tmp/bad.c .')
    c('cc -DVAL=3 -I/tmp main.c twice.c',stdout='main.c:\ntwice.c:\n')
    c('./a.out',stdout=OUT)
    c('test -f main.o',status=0); c('test -f twice.o')
    c('cc -c -O twice.c; cc -o prog -DVAL=3 -I/tmp main.c twice.o')
    c('./prog',stdout=OUT)
    c('cc -S -O twice.c; grep -c ret twice.s',stdout='1\n')
    c('cc -o prog -DVAL=3 -I/tmp main.c one.s',stdout='main.c:\none.s:\n')
    c('./prog',stdout=OUT)
    c('cc -P -DVAL=3 -I/tmp main.c; grep -c "cc %d" main.i',stdout='1\n')
    c('cc -E -DVAL=7 -I/tmp main.c | grep -c "d = 7"',stdout='1\n')
    c('cc -f -o fprog -DVAL=3 -I/tmp main.c twice.c',stdout='main.c:\ntwice.c:\n')
    c('./fprog',stdout=OUT)
    c('cc -p -o pprog -DVAL=3 -I/tmp main.c twice.c',stdout='main.c:\ntwice.c:\n')
    c('./pprog',stdout=OUT)
    c('test -s mon.out')
    c('mkdir alt; cp /lib/c0 alt/c0; cc -t0 -B./alt/ -o tprog -DVAL=3 -I/tmp main.c twice.o')
    c('./tprog',stdout=OUT)
    c('rm -f a.out; cc bad.c 2> err; echo $?',stdout='1\n')
    c('grep -c "Expression syntax" err',stdout='1\n')
    c('test -f a.out',status=1)
    c('cc -o twice.c main.c',stdout='Would overwrite twice.c\n',status=8)
    c('cd /')
with private_guest(qemu,iso,ram,'cc',art,terminals=2,single_user=False) as g:
    login(g,'root','nexix')
    run(g,'/tmp/r',0)
    login(g,'bob','bob',line=1,prompt='$ ')
    run(g,'/tmp/u',1)
print(f'PASS: {ram} installed cc driver (-c, -S, -O, -P, -E, -f, -p, -o, -D/-I, -t/-B, .s input, failures) as root and ordinary user')

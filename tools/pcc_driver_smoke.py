"""Installed /bin/pcc and /usr/lib/ccom, and the native fort second pass."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F='fixtures-pcc-driver/'
artifacts={'/tmp/hello.c':F+'hello.c','/tmp/twice.c':F+'twice.c','/tmp/defs.h':F+'defs.h','/tmp/asm.s':F+'asm.s',
    '/tmp/bad.c':F+'bad.c','/tmp/fort':'pcc-fort.elf','/tmp/fort.int':F+'fort.int','/tmp/fortdrv.o':F+'fortdriver.o'}
OUT='hello 3 6 42\n'
def run(guest,d,t):
    c=lambda line,**k:guest.command(line,terminal=t,**k)
    c(f'mkdir {d}; cd {d}')
    c('pcc -DVAL=3 -I/tmp /tmp/hello.c /tmp/twice.c',stdout='hello.c\ntwice.c\n')
    c('./a.out',stdout=OUT)
    c('pcc -c /tmp/twice.c',stdout='twice.c\n')
    c('pcc -O -DVAL=3 -I/tmp /tmp/hello.c twice.o',stdout='hello.c\n')
    c('./a.out',stdout=OUT)
    c('pcc -X -DVAL=3 -I/tmp /tmp/hello.c /tmp/asm.s',stdout='hello.c\nasm.s:\n')
    c('./a.out',stdout=OUT)
    c('pcc -S /tmp/twice.c; grep -c "# line" twice.s',stdout='twice.c\n1\n')
    c('pcc -S -O /tmp/twice.c; grep -c "# line" twice.s',stdout='twice.c\n1\n')
    c('pcc -p - -DVAL=3 -I/tmp /tmp/hello.c twice.o; nm a.out | grep -c " mcount"',stdout='1\n')
    c('./a.out',stdout=OUT)
    c('rm a.out; pcc /tmp/bad.c 2> err',stdout='bad.c\n',status=1)
    c('test -f a.out',status=1)
    c('/tmp/fort /tmp/fort.int fort.s; as -o fort.o fort.s')
    c('ld -o fortprog /lib/crt0.o /tmp/fortdrv.o fort.o -lc; ./fortprog',stdout='PASS: native fort second pass\n')
    c('cd /')
with private_guest(qemu,iso,ram,'pcc-driver',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    run(guest,'/tmp/r',0)
    login(guest,'bob','bob',line=1,prompt='$ ')
    run(guest,'/tmp/u',1)
print(f'PASS: {ram} installed pcc driver (multi-file, -c, -O, -X, -S, -p, -D/-I, .s input, errors), ccom and fort as root and ordinary user')

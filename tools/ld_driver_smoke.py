"""Installed native assembler/archive/linker pipeline and runnable libc consumers."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={}
for short,name in (('cmd.o','v7-ld.o'),('res.o','ld-resolve.o'),('out.o','ld-output.o'),('rel.o','ld-relocatable.o'),('obj.o','toolchain-object.o'),('arc.o','toolchain-archive.o')):
    artifacts['/tmp/'+short]=name
for name in ('native.c','native.o','native.elf','native-part.o','calculate.s','hoststart.o','relocs.o','wide.o','invalid.bad'):
    artifacts['/tmp/'+name]='fixtures-ld/'+name
with private_guest(qemu,iso,ram,'ld-driver',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('ld -o /tmp/native /lib/crt0.o /tmp/native.o /lib/libc.a')
    guest.command('cmp /tmp/native /tmp/native.elf')
    guest.command('/tmp/native',stdout='PASS: native linked C and libc\n')
    guest.command('ld -r -o /tmp/partial.o /lib/crt0.o /tmp/native.o /lib/libc.a')
    guest.command('cmp /tmp/partial.o /tmp/native-part.o')
    guest.command('ld -o /tmp/relinked /tmp/partial.o')
    guest.command('/tmp/relinked',stdout='PASS: native linked C and libc\n')
    guest.command('ld -s -o /tmp/stripped /tmp/partial.o')
    guest.command('/tmp/stripped',stdout='PASS: native linked C and libc\n')
    guest.command('as -o /tmp/calculate.o /tmp/calculate.s')
    guest.command('ar rcs /tmp/libcalculate.a /tmp/calculate.o')
    guest.command('ranlib /tmp/libcalculate.a')
    guest.command('ld -L/tmp -o /tmp/asmlinked /tmp/hoststart.o -lcalculate')
    guest.command('ld -o /tmp/selfld /lib/crt0.o /tmp/cmd.o /tmp/res.o /tmp/out.o /tmp/rel.o /tmp/obj.o /tmp/arc.o -lc')
    guest.command('/tmp/selfld -o /tmp/selfrun /lib/crt0.o /tmp/native.o -lc')
    guest.command('/tmp/selfrun',stdout='PASS: native linked C and libc\n')
    guest.command('cp /tmp/native /tmp/original')
    guest.command('ld -o /tmp/native /tmp/relocs.o /tmp/wide.o',stdout='ld: x86_64 relocation overflow\n',status=1)
    guest.command('cmp /tmp/native /tmp/original')
    guest.command('ld -o /tmp/native /tmp/invalid.bad',stdout='ld: /tmp/invalid.bad: invalid ELF64 relocatable object\n',status=1)
    guest.command('cmp /tmp/native /tmp/original')
    guest.command('ld -o /tmp/native /tmp/hoststart.o',stdout='ld: undefined symbol: calculate\n',status=1)
    guest.command('cmp /tmp/native /tmp/original')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('ld -o /tmp/bob /lib/crt0.o /tmp/native.o -lc',terminal=1)
    guest.command('/tmp/bob',terminal=1,stdout='PASS: native linked C and libc\n')
    guest.command('ld -r -o /tmp/bob.o /lib/crt0.o /tmp/native.o -lc',terminal=1)
    guest.command('ld -o /tmp/bob-again /tmp/bob.o',terminal=1)
    guest.command('/tmp/bob-again',terminal=1,stdout='PASS: native linked C and libc\n')
print(f'PASS: {ram} installed ld, assembler/archive pipeline, -r/strip relinks, native libc execution, self-link, errors and ordinary-user builds')

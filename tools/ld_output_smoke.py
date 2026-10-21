"""Produce and run static ELF inside Nexix using the production link passes."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/ldoutput':'ld-output-test.elf','/tmp/crt0.o':'crt0.o','/tmp/libc.a':'libc.a'}
for name in ('native.o','native.elf','hoststart.o','calculate.o','calculate.s','relocs.o','wide.o'):
    artifacts['/tmp/'+name]='fixtures-ld/'+name
with private_guest(qemu,iso,ram,'ld-output',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/tmp/ldoutput _start /tmp/linked /tmp/crt0.o /tmp/native.o /tmp/libc.a')
    guest.command('cmp /tmp/linked /tmp/native.elf')
    guest.command('/tmp/linked',stdout='PASS: native linked C and libc\n')
    guest.command("nm -g /tmp/linked | grep -c ' T main$'",stdout='1\n')
    guest.command('cp /tmp/linked /tmp/before')
    guest.command('/tmp/ldoutput _start /tmp/linked /tmp/relocs.o /tmp/wide.o',stdout='ld-output: x86_64 relocation overflow\n',status=1)
    guest.command('cmp /tmp/linked /tmp/before')
    # Consume an object produced by the installed native assembler in this guest.
    guest.command('as -o /tmp/calculated.o /tmp/calculate.s')
    guest.command('/tmp/ldoutput _start /tmp/fromas /tmp/hoststart.o /tmp/calculated.o')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/ldoutput _start /tmp/boblinked /tmp/crt0.o /tmp/native.o /tmp/libc.a',terminal=1)
    guest.command('cmp /tmp/boblinked /tmp/native.elf',terminal=1)
    guest.command('/tmp/boblinked',terminal=1,stdout='PASS: native linked C and libc\n')
print(f'PASS: {ram} native ELF layout/relocations, exact production, C/libc execution, native assembler input and failure preservation')

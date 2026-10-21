"""Compile and execute retained-PCC integer C through installed native as/ld."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
import json
from pathlib import Path
modules=json.loads(Path('build/fixtures-pcc/modules.json').read_text())
artifacts={'/tmp/pccc':'pcc-backend-test.elf','/tmp/caller.o':'fixtures-pcc/int-driver.o',
    '/tmp/prog.c':'fixtures-pcc/program.c','/tmp/prog.exp':'fixtures-pcc/program.s'}
for i,name in enumerate(modules):
    artifacts[f'/tmp/p{i}.c']='fixtures-pcc/'+name+'.c'
    artifacts[f'/tmp/p{i}.exp']='fixtures-pcc/'+name+'.s'
with private_guest(qemu,iso,ram,'pcc-integer',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    for i in range(len(modules)):
        guest.command(f'/lib/cpp /tmp/p{i}.c /tmp/p{i}.i')
        guest.command(f'/tmp/pccc /tmp/p{i}.i > /tmp/p{i}.s 2> /tmp/warnings')
        guest.command(f'cmp /tmp/p{i}.s /tmp/p{i}.exp')
        guest.command(f'as -o /tmp/p{i}.o /tmp/p{i}.s')
    objects=' '.join(f'/tmp/p{i}.o' for i in range(len(modules)))
    guest.command('ld -o /tmp/inttest /lib/crt0.o /tmp/caller.o '+objects+' -lc')
    guest.command('/tmp/inttest',stdout='PASS: PCC integer/pointer/control backend\n')
    guest.command('/lib/cpp /tmp/prog.c /tmp/prog.i')
    guest.command('/tmp/pccc /tmp/prog.i > /tmp/prog.s 2> /tmp/warnings')
    guest.command('cmp /tmp/prog.s /tmp/prog.exp')
    guest.command('as -o /tmp/prog.o /tmp/prog.s')
    guest.command('ld -o /tmp/program /lib/crt0.o /tmp/prog.o '+objects+' -lc')
    guest.command('/tmp/program')
    login(guest,'bob','bob',line=1,prompt='$ ')
    for i in range(len(modules)):
        guest.command(f'/lib/cpp /tmp/p{i}.c /tmp/b{i}.i',terminal=1)
        guest.command(f'/tmp/pccc /tmp/b{i}.i > /tmp/b{i}.s 2> /tmp/bobwarn',terminal=1)
        guest.command(f'cmp /tmp/b{i}.s /tmp/p{i}.exp',terminal=1)
        guest.command(f'as -o /tmp/b{i}.o /tmp/b{i}.s',terminal=1)
    objects=' '.join(f'/tmp/b{i}.o' for i in range(len(modules)))
    guest.command('ld -o /tmp/bobtest /lib/crt0.o /tmp/caller.o '+objects+' -lc',terminal=1)
    guest.command('/tmp/bobtest',terminal=1,stdout='PASS: PCC integer/pointer/control backend\n')
print(f'PASS: {ram} native cpp/PCC/as/ld integer pipeline, exact assembly, mixed ABI execution and ordinary-user compilation')

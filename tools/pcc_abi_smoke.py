"""Native cpp/PCC/as/ld aggregate, long-double and variadic ABI execution."""
import sys
import json
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
modules=json.loads(Path('build/fixtures-pcc-abi/modules.json').read_text())
artifacts={'/tmp/pccc':'pcc-backend-test.elf','/tmp/native.h':'fixtures-pcc-abi/native.h','/tmp/abidrv.o':'fixtures-pcc-abi/driver.o'}
for i,name in enumerate(modules):
    artifacts[f'/tmp/a{i}.c']='fixtures-pcc-abi/'+name+'.c'
    artifacts[f'/tmp/a{i}.exp']='fixtures-pcc-abi/'+name+'.s'
PASS='PASS: PCC aggregate/long-double/variadic ABI\n'
def pipeline(guest,prefix,terminal):
    for i in range(len(modules)):
        guest.command(f'/lib/cpp /tmp/a{i}.c /tmp/{prefix}{i}.i',terminal=terminal)
        guest.command(f'/tmp/pccc --native-source /tmp/{prefix}{i}.i > /tmp/{prefix}{i}.s',terminal=terminal)
        guest.command(f'cmp /tmp/{prefix}{i}.s /tmp/a{i}.exp',terminal=terminal)
        guest.command(f'/tmp/pccc --emit-pass --native-source /tmp/{prefix}{i}.i > /tmp/{prefix}pass',terminal=terminal)
        guest.command(f'/tmp/pccc --read-pass /tmp/{prefix}pass > /tmp/{prefix}split.s',terminal=terminal)
        guest.command(f'cmp /tmp/{prefix}split.s /tmp/{prefix}{i}.s',terminal=terminal)
        guest.command(f'as -o /tmp/{prefix}{i}.o /tmp/{prefix}split.s',terminal=terminal)
    groups=[]
    for start in range(0,len(modules),4):
        group=f'/tmp/{prefix}g{start//4}.o';groups.append(group)
        guest.command('ld -r -o '+group+' '+' '.join(f'/tmp/{prefix}{i}.o' for i in range(start,min(start+4,len(modules)))),terminal=terminal)
    guest.command(f'ld -o /tmp/{prefix}abi /lib/crt0.o /tmp/abidrv.o '+' '.join(groups)+' -lc',terminal=terminal)
    guest.command(f'/tmp/{prefix}abi',terminal=terminal,stdout=PASS)
with private_guest(qemu,iso,ram,'pcc-abi',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    pipeline(guest,'r',0)
    login(guest,'bob','bob',line=1,prompt='$ ')
    pipeline(guest,'u',1)
print(f'PASS: {ram} native cpp/PCC/as/ld aggregate, long-double and variadic ABI with exact assembly and ordinary-user compilation')

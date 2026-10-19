"""Verify real kernel identity and installed uname for root and ordinary users."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'uname',{'/tmp/uname-api':'uname-api.elf'},2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('uname',stdout='Nexix\n')
    guest.command('uname -a',stdout='Nexix nexix.localdomain 0.1.0 development x86_64\n')
    for flag,value in zip('snrvm',['Nexix','nexix.localdomain','0.1.0','development','x86_64']):
        guest.command('uname -'+flag,stdout=value+'\n')
    guest.command('uname -m -r -s -s',stdout='Nexix 0.1.0 x86_64\n')
    guest.command('uname --',stdout='Nexix\n')
    guest.command('uname -z',stdout='usage: uname [-asnrvm]\n',status=2)
    guest.command('uname file',stdout='usage: uname [-asnrvm]\n',status=2)
    guest.command('/tmp/uname-api',stdout='UNAME_API_OK\n')
    login(guest,'alice','alice',1,prompt='$ ')
    guest.command('uname -a',1,stdout='Nexix nexix.localdomain 0.1.0 development x86_64\n',prompt=r'\$ ')
    guest.command('/tmp/uname-api',1,stdout='UNAME_API_OK\n',prompt=r'\$ ')
print(f'PASS: {ram} uname kernel/libc ABI, all fields/options, bad/read-only addresses and unprivileged access')

"""Run production mip contracts in both native memory configurations."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'mip-common',{'/tmp/miptest':'mip-common-test.elf'},terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/tmp/miptest',stdout='PASS: native mip types, operators and bounded IR\n')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/miptest',terminal=1,stdout='PASS: native mip types, operators and bounded IR\n')
print(f'PASS: {ram} native mip common contracts as root and ordinary user')

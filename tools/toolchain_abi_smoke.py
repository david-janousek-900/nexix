"""Run the versioned C ABI fixture against the real guest startup/libc."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu, iso, ram = sys.argv[1:4]
with private_guest(qemu,iso,ram,'toolchain-abi',{'/tmp/abi':'toolchain-abi.elf'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('ABI_TEST=yes /tmp/abi native',stdout='TOOLCHAIN_ABI_OK\n')
print(f'PASS: {ram} LP64 layouts, independent assembly registers/stack/AL, aggregate and variadic calls')

"""Run private clock/sleep/syscall ABI acceptance without installing fixtures."""
import sys
from private_guest import private_guest
qemu,iso,ram,name,marker=sys.argv[1:6]
with private_guest(qemu,iso,ram,name,{'/tmp/timing':name+'.elf'}) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.command('/tmp/timing',stdout=marker+'\n')
print(f'PASS: {ram} {name} guest acceptance')

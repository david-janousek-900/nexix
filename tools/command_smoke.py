"""Run a private native state-inspection fixture against an installed command."""
import sys
from private_guest import private_guest
qemu,iso,ram,name=sys.argv[1:5]
with private_guest(qemu,iso,ram,name,{'/bin/check':name+'-command.elf'}) as guest:
    guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
    guest.command('/bin/check',stdout=name.upper()+'_OK\n')
print(f'PASS: {ram} installed {name}: output/status, native metadata and failure fixtures')

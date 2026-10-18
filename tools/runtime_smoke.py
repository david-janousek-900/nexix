"""Native runtime replacement acceptance on a private full root."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'runtime',{'/tmp/runtime':'runtime-equivalent.elf'}) as guest:
    guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
    guest.command('/tmp/runtime',stdout='RUNTIME_EQUIVALENT_OK\n')
print(f'PASS: {ram} native compiler arithmetic/prologues, varargs and V6 API replacements')

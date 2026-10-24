"""Exercise installed Lite2 network databases through the production libc."""
import sys
from private_guest import private_guest
qemu, iso, ram = sys.argv[1:4]
with private_guest(qemu, iso, ram, 'netdb', {'/tmp/netdb': 'netdb-guest.elf'},
                   extra_args=('-nic', 'none')) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.command('/tmp/netdb', stdout='NETDB_GUEST_OK\n')
print(f'PASS: {ram} installed network database libc lookups')

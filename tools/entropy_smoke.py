"""Kernel entropy success, fork independence and absent-source behavior."""
import sys
from private_guest import private_guest
qemu, iso, ram = sys.argv[1:4]
for available in (True, False):
    extra=('-nic','none') if available else ('-nic','none','-cpu','qemu64,-rdrand')
    with private_guest(qemu, iso, ram, 'entropy' if available else 'entropy-absent',
                       {'/tmp/entropy':'entropy-guest.elf'}, extra_args=extra) as guest:
        guest.expect(r'root@[A-Za-z0-9.-]+# ')
        guest.command('/tmp/entropy'+('' if available else ' absent'),
                      stdout='ENTROPY_GUEST_OK\n' if available else 'ENTROPY_UNAVAILABLE_OK\n')
print(f'PASS: {ram} entropy, query IDs, distribution, fork and unavailable source')

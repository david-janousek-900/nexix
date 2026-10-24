"""Resolver transport against a DNS server inside a disposable guest."""
import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'resolver',{'/tmp/resolver':'resolver-guest.elf'},extra_args=('-nic','none')) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.timeout=40
    guest.command('/tmp/resolver',stdout='RESOLVER_GUEST_OK\n')
print(f'PASS: {ram} resolver UDP/TCP, reply matching, drain, errors and timeouts')

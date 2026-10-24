"""Resolver configuration via the installed libc and native files/hostname."""
import sys
from private_guest import private_guest
qemu, iso, ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'res-init',{'/tmp/res-init':'res-init-guest.elf'},extra_args=('-nic','none')) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.command('/tmp/res-init',stdout='RES_INIT_GUEST_OK\n')
print(f'PASS: {ram} resolver files, search, hostname and LOCALDOMAIN')

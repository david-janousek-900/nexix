"""Installed accton controls real cross-user process accounting."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login

qemu, iso, ram = sys.argv[1:4]
with private_guest(qemu, iso, ram, 'accton', {'/tmp/acctcheck': 'accton-records.elf'},
                   single_user=False, terminals=2) as guest:
    login(guest, 'root', 'nexix')
    login(guest, 'bob', 'bob', 1, '$ ')
    guest.command('/etc/accton')
    guest.command('/etc/accton /tmp/absent', stdout='accton: No such file or directory\n', status=1)
    guest.command('/etc/accton /tmp', stdout='accton: Permission denied\n', status=1)
    guest.command('cp /dev/null /tmp/account')
    guest.command('/etc/accton /tmp/account ignored')
    guest.command('/etc/accton /tmp/account', stdout='accton: Mount device busy\n', status=1)
    guest.command('/etc/accton', 1, stdout='accton: Not owner\n', status=1, prompt=r'\$ ')
    guest.command('sum /dev/null', 1, stdout='00000     0\n', prompt=r'\$ ')
    guest.command('/etc/accton')
    guest.command('/tmp/acctcheck /tmp/account', stdout='ACCTON_RECORDS_OK\n')
    guest.command('cp /tmp/account /tmp/saved')
    guest.command('sum /dev/null', stdout='00000     0\n')
    guest.command('cmp /tmp/account /tmp/saved')
    guest.command('/etc/accton')
print(f'PASS: {ram} accton enable/disable, privileges, errors and actual native records')

"""Shared native mip template/pass consumer through installed as and ld."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/mipcore':'mip-infra-test.elf','/tmp/expected.s':'fixtures-mip/generated.s',
    '/tmp/pass.ir':'fixtures-mip/pass.ir','/tmp/driver.o':'fixtures-mip/driver.o'}
with private_guest(qemu,iso,ram,'mip-infra',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    expected='PASS: mip templates, resources, traversal, diagnostics and pass interface\n'
    guest.command('/tmp/mipcore /tmp/mip.s /tmp/mip.ir /tmp/mip.log',stdout=expected)
    guest.command('cmp /tmp/mip.s /tmp/expected.s')
    guest.command('cmp /tmp/mip.ir /tmp/pass.ir')
    guest.command('as -o /tmp/mip.o /tmp/mip.s')
    guest.command('ld -o /tmp/miprun /lib/crt0.o /tmp/driver.o /tmp/mip.o -lc')
    guest.command('/tmp/miprun',stdout='PASS: shared mip template pipeline\n')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/mipcore /tmp/bob.s /tmp/bob.ir /tmp/bob.log',terminal=1,stdout=expected)
    guest.command('cmp /tmp/bob.ir /tmp/pass.ir',terminal=1)
    guest.command('as -o /tmp/bob.o /tmp/bob.s',terminal=1)
    guest.command('ld -o /tmp/bobrun /lib/crt0.o /tmp/driver.o /tmp/bob.o -lc',terminal=1)
    guest.command('/tmp/bobrun',terminal=1,stdout='PASS: shared mip template pipeline\n')
print(f'PASS: {ram} mip templates/resources/pass interface, exact native bytes and generated-code execution as root/user')

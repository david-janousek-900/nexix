"""Native parser-independent mip transport and analysis consumer."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/mippass':'mip-pass-test.elf','/tmp/alternate':'fixtures-mip/alternate.pass',
           '/tmp/frompcc':'fixtures-pcc/integer.pass','/tmp/analysis':'fixtures-mip/pcc-analysis'}
with private_guest(qemu,iso,ram,'mip-pass',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    expected='PASS: shared mip stream validation and independent analysis consumer\n'
    guest.command('/tmp/mippass /tmp/nativepass /tmp/scratch',stdout=expected)
    guest.command('cmp /tmp/nativepass /tmp/alternate')
    guest.command('/tmp/mippass --check /tmp/frompcc > /tmp/check')
    guest.command('cmp /tmp/check /tmp/analysis')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/mippass /tmp/bobpass /tmp/bscratch',terminal=1,stdout=expected)
    guest.command('cmp /tmp/bobpass /tmp/alternate',terminal=1)
    guest.command('/tmp/mippass --check /tmp/frompcc > /tmp/bcheck',terminal=1)
    guest.command('cmp /tmp/bcheck /tmp/analysis',terminal=1)
print(f'PASS: {ram} shared mip stream, failure cases, alternate producer and independent PCC analysis as root/user')

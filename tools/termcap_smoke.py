import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'termcap',{'/tmp/termcapcheck':'termcap-guest.elf'}) as guest:
 guest.expect(r'root@nexix.localdomain# ')
 guest.command('/tmp/termcapcheck',stdout='TERMCAP_OK\n')
print(f'PASS: {ram} native termcap filesystem/lookup/decoding')

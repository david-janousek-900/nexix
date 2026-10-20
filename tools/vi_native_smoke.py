import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'vi-native',{'/tmp/vinative':'vi-native.elf','/tmp/ex':'vi-dev.elf'}) as guest:
 guest.expect(r'root@nexix.localdomain# ')
 guest.command('/tmp/vinative',stdout='VI_NATIVE_OK\n')
 guest.command("echo q! | TERMCAP='dumb:co#80:li#24:' /tmp/ex -",stdout='')
print(f'PASS: {ram} native vi ABI adapters and linked editor startup/exit')

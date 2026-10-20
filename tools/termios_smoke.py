import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'termios',{'/tmp/termioscheck':'termios-guest.elf'}) as guest:
 guest.expect(r'root@nexix.localdomain# ')
 guest.send('/tmp/termioscheck\n');guest.expect('TERMIOS_READY\r\n')
 guest.send(b'\x03');guest.expect('TERMIOS_OK\r\n');guest.expect(r'root@nexix.localdomain# ')
 guest.command('echo restored',stdout='restored\n')
print(f'PASS: {ram} native terminal geometry, mode validation, interrupt and restoration')

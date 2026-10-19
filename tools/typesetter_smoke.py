"""Typesetter device acceptance on a private full root."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'typesetter',{'/tmp/typesetter':'typesetter.elf'}) as guest:
    guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
    guest.command('/tmp/typesetter',stdout='TYPESETTER_OK\n')
print(f'PASS: {ram} typesetter binary capture, exclusive open, bad pointers, ioctl and descriptor lifetime')

"""Printer/plotter acceptance on a private full root."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'printer',{'/tmp/printer':'printer.elf'}) as guest:
    guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+ # ')
    guest.command('/tmp/printer',stdout='PRINTER_OK\n')
print(f'PASS: {ram} VP command/mode framing and PPLOT transition; VS burst/input protocol')

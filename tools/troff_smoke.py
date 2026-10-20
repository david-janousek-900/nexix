"""Troff ASCII/protocol output, mounted font data and original manual macros."""
import sys
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
Path('build/troff-input').write_text('''.nf
Troff fixture
.ft B
Bold text
.ft I
Italic text
.ft R
.ps 12
Point size twelve
.fp 1 CW
.ft 1
Mounted font
''')
with private_guest(qemu,iso,ram,'troff',{'/tmp/troff':'v7-troff.elf','/tmp/input':'troff-input','/tmp/check':'formatter-protocol.elf'}) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command('/tmp/troff -a /tmp/input > /tmp/plain')
 g.command("grep -c 'Troff fixture' /tmp/plain",stdout='1\n')
 g.command('/tmp/troff -t /tmp/input > /tmp/protocol')
 g.command('/tmp/check cat /tmp/protocol',stdout='PROTOCOL_OK\n')
 g.command('/tmp/troff -t -man /usr/man/man1/cat.1 > /tmp/protocol')
 g.command('/tmp/check cat /tmp/protocol',stdout='PROTOCOL_OK\n')
 g.command('/tmp/troff -a -man /usr/man/man1/cat.1 > /tmp/plain')
 g.command("grep -c 'concatenate' /tmp/plain",stdout='1\n')
print('PASS: '+ram+' troff ASCII, CAT protocol, size/font switching, native mounted font and manual macros')

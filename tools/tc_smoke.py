"""Translate captured CAT into Tektronix bytes without requiring hardware."""
import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'tc',{'/tmp/tc':'v7-tc.elf','/tmp/check':'formatter-protocol.elf'}) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command('troff -t -man /usr/man/man1/cat.1 > /tmp/catout')
 g.command('/tmp/tc -t /tmp/catout > /tmp/tekout')
 g.command('/tmp/check tek /tmp/tekout',stdout='PROTOCOL_OK\n')
 g.command('/tmp/tc -t -p11 -2/1 /tmp/catout > /tmp/tekout')
 g.command('/tmp/check tek /tmp/tekout',stdout='PROTOCOL_OK\n')
 g.command('/tmp/tc -t /tmp/catout 1>&-',stdout='tc: output error\n',status=1)
 g.command('/tmp/tc /tmp/absent',stdout='tc: cannot open /tmp/absent\n',status=1)
print('PASS: '+ram+' tc CAT-to-Tek framing, page/scale options and missing input')

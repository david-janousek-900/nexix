"""Target execution of retained font-table helpers, including malformed input."""
import sys
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
Path('build/roff-widths').write_text('12\n'*102)
with private_guest(qemu,iso,ram,'formatter-tables',{'/tmp/mkfont':'roff-mkfont.elf','/tmp/chars':'roff-chars.elf','/tmp/widths':'roff-widths'}) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command('/tmp/mkfont /tmp/widths > /tmp/font')
 g.command("grep 'char XXw' /tmp/font",stdout='char XXw[256-32] = {\t/*XX*/\n')
 g.command('/tmp/mkfont < /dev/null > /tmp/font',stdout='mkfont: expected 102 widths\n',status=1)
 g.command('/tmp/chars > /tmp/charsout')
 g.command("grep 'Not-a-character' /tmp/charsout",status=1)
 g.command('test -s /tmp/charsout')
print('PASS: '+ram+' native mkfont/chars and invalid input')

"""Exercise V7 macro arguments, environments, registers and diversions natively."""
import sys
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
Path('build/roff-fixture').write_text(r'''.nf
.na
.hy 0
.de NX
ARG=\\$1
..
.NX native
.nr xx 42
REGISTER=\n(xx
.ev 1
.nf
ENVIRONMENT=one
.ev
ENVIRONMENT=zero
.di ZZ
DIVERTED=text
.di
.ZZ
''')
with private_guest(qemu,iso,ram,'formatter',{'/tmp/nroff':'v7-nroff.elf','/tmp/input':'roff-fixture'}) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command("echo 'Hello Nexix' | /tmp/nroff -Tlp > /tmp/out")
 g.command("grep 'Hello Nexix' /tmp/out",stdout='Hello Nexix\n')
 g.command('/tmp/nroff -Tlp /tmp/input > /tmp/out')
 for line in ['ARG=native','REGISTER=42','ENVIRONMENT=one','ENVIRONMENT=zero','DIVERTED=text']:
  g.command("grep '"+line+"' /tmp/out",stdout=line+'\n')
 g.command('/tmp/nroff -Tmissing /tmp/input > /tmp/out',stdout='Cannot open /usr/lib/term/tabmissing\n',status=255)
print('PASS: '+ram+' native V7 text, macros, argument frames, environments, registers and diversions')

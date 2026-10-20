"""Render every installed V7 manual in sections 1–8 on the target."""
import sys
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'nroff',{}) as g:
 g.expect(r'root@nexix.localdomain# ');g.command('cd /usr/man')
 pages=[p for p in sorted(Path('reference/v7/usr/man').glob('man[1-8]/*')) if p.is_file()]
 for index,page in enumerate(pages):
  relative=str(page.relative_to('reference/v7/usr/man'))
  g.command('nroff -man '+relative+' > /tmp/page')
  if index%25==0:print(f'{ram}: rendered {index+1}/{len(pages)} through {relative}',flush=True)
 g.command('nroff -man man1/cat.1 > /tmp/page')
 g.command("grep -c 'concatenate' /tmp/page",stdout='1\n')
 g.command('nroff -Tbad man1/cat.1 > /tmp/page',stdout='Cannot open /usr/lib/term/tabbad\n',status=255)
print(f'PASS: {ram} native nroff renders all {len(pages)} historical section 1–8 manual files')

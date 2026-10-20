"""Installed public character tables and equation-definition consumers."""
import sys,json
from pathlib import Path
from private_guest import private_guest
root=Path(__file__).resolve().parents[1]
entries={e['path']:e for e in json.loads((root/'docs/native-install.json').read_text())['entries']}
for source in (root/'reference/v7/usr/pub').iterdir():
 entry=entries['/usr/pub/'+source.name]
 assert entry['component']=='distribution.public' and entry['mode']=='0644'
 assert (root/'rootfs'/entry['source']['path']).read_bytes()==source.read_bytes()
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'public-data',{}) as g:
 g.expect(r'root@nexix.localdomain# ')
 for filename in ['ascii','greek']:g.command('test -s /usr/pub/'+filename)
 for program,formatter in [('neqn','nroff'),('eqn','troff -a')]:
  g.command(program+' /usr/pub/eqnchar /usr/man/man7/eqnchar.7 > /tmp/equations')
  g.command(formatter+' -man /tmp/equations > /tmp/characters')
  g.command('test -s /tmp/characters')
print('PASS: '+ram+' unchanged ASCII/Greek/equation data and native eqn/neqn consumers')

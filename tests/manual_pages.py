"""Every historical manual is staged byte-for-byte with explicit ownership,
except the declared pages whose binary paths follow the BSD layout (task 432):
those must equal the original with exactly these substitutions."""
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
entries={e['path']:e for e in json.loads((root/'docs/native-install.json').read_text())['entries']}
files=sorted((root/'reference/v7/usr/man').rglob('*'))
files=[p for p in files if p.is_file()]
assert len(files)==307
MOVED={'/etc/init':'/sbin/init','/etc/mknod':'/sbin/mknod','/etc/accton':'/usr/sbin/accton',
       '/etc/update':'/usr/sbin/update','/etc/getty':'/usr/libexec/getty','/etc/wall':'/usr/bin/wall'}
ADAPTED={'/usr/man/man1/mknod.1m','/usr/man/man1/sa.1m','/usr/man/man1/wall.1m','/usr/man/man8/crash.8',
         '/usr/man/man8/getty.8','/usr/man/man8/init.8','/usr/man/man8/update.8'}
def relocated(text):
    for old,new in MOVED.items():text=re.sub(re.escape(old).encode()+rb'(?![A-Za-z0-9_.])',new.encode(),text)
    return text
for source in files:
 destination='/'+str(source.relative_to(root/'reference/v7'))
 entry=entries[destination]
 assert entry['component']=='distribution.manuals' and entry['mode']=='0644'
 installed=(root/'rootfs'/entry['source']['path']).read_bytes()
 if destination in ADAPTED:
  assert installed!=source.read_bytes() and installed==relocated(source.read_bytes()),destination
 else:
  assert installed==source.read_bytes() and relocated(installed)==installed,destination   # nothing left to move
print(f'PASS: all 307 V7 manual files, modes and ownership; {307-len(ADAPTED)} byte-identical, '
      f'{len(ADAPTED)} differing only by BSD binary paths')

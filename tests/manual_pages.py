"""Every historical manual is staged byte-for-byte with explicit ownership."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
entries={e['path']:e for e in json.loads((root/'docs/native-install.json').read_text())['entries']}
files=sorted((root/'reference/v7/usr/man').rglob('*'))
files=[p for p in files if p.is_file()]
assert len(files)==307
for source in files:
 destination='/'+str(source.relative_to(root/'reference/v7'))
 entry=entries[destination]
 assert entry['component']=='distribution.manuals' and entry['mode']=='0644'
 assert (root/'rootfs'/entry['source']['path']).read_bytes()==source.read_bytes(),destination
print('PASS: all 307 V7 manual files, modes, ownership and byte fidelity')

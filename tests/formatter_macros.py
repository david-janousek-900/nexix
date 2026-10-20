"""Verify original macros and parser/library descriptions are installed intact."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
entries={e['path']:e for e in json.loads((root/'docs/native-install.json').read_text())['entries']}
files=list((root/'reference/v7/usr/lib/tmac').iterdir())+[root/'reference/v7/usr/lib'/p for p in ['yaccpar','lex/ncform','llib-lc','llib-lm','llib-port']]
for src in files:
 path='/'+str(src.relative_to(root/'reference/v7'));e=entries[path]
 assert e['mode']=='0644' and e['component']=='distribution.library-data'
 assert (root/'rootfs'/e['source']['path']).read_bytes()==src.read_bytes(),path
print('PASS: all formatter macros and parser/library descriptions installed intact')

"""Verify unchanged formatter macros and installed library data ownership.
Native yaccpar, ncform and lint descriptions are adapted and exercised by
tests/yacc.py, tests/lex.py and tests/lint.py."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
entries={e['path']:e for e in json.loads((root/'docs/native-install.json').read_text())['entries']}
files=list((root/'reference/v7/usr/lib/tmac').iterdir())+[root/'reference/v7/usr/lib'/p for p in ['lex/ncform','llib-lc','llib-lm','llib-port']]
for src in files:
 path='/'+str(src.relative_to(root/'reference/v7'));e=entries[path]
 assert e['mode']=='0644' and e['component']=='distribution.library-data'
 assert 'tmac' not in path or (root/'rootfs'/e['source']['path']).read_bytes()==src.read_bytes(),path
print('PASS: formatter macros installed intact; lex skeleton and lint descriptions installed')

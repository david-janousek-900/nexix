"""Check portable regenerated terminals and font extents."""
from pathlib import Path
import struct
root=Path(__file__).resolve().parents[1];base=root/'build/roff-data'
term=list((root/'reference/v7/usr/src/cmd/troff/term').glob('tab*.c'))
for src in term:
 data=(base/src.stem).read_bytes();assert data[:8]==b'NXTERM1\0'
 metrics=struct.unpack_from('<9i',data,8);assert min(metrics[2:])>0
 pos=44
 for i in range(238):
  size=struct.unpack_from('<I',data,pos)[0];pos+=4
  assert size<=1024 and pos+size<=len(data);pos+=size
 assert pos==len(data),(src,pos,len(data))
fonts=list((root/'reference/v7/usr/src/cmd/troff/font').glob('ft*.c'))
for src in fonts:assert (base/src.stem).stat().st_size==224
print(f'PASS: {len(term)} native terminal tables and {len(fonts)} font width tables')

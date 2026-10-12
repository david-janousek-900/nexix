"""Independently inspect the host-produced ELF core and its documented notes."""
from pathlib import Path
import struct
import subprocess
data=Path('build/core-fixture.elf').read_bytes()
assert data[:8]==b'\x7fELF\x02\x01\x01\x00'
h=struct.unpack_from('<16sHHIQQQIHHHHHH',data)
assert h[1:4]==(4,62,1) and h[8:11]==(64,56,3)
ph=[struct.unpack_from('<IIQQQQQQ',data,h[5]+i*56) for i in range(h[10])]
assert [p[0] for p in ph]==[4,1,1]
for p in ph[1:]:
    assert p[1]==6 and p[4]==0 and p[5]==p[6] and p[7]==4096
    assert p[2]%p[7]==p[3]%p[7] and p[2]+p[5]<=len(data)
notes={};pos=ph[0][2];end=pos+ph[0][5]
while pos<end:
    namesz,size,kind=struct.unpack_from('<III',data,pos);pos+=12
    assert namesz==6 and data[pos:pos+namesz]==b'NEXIX\0'
    pos+=(namesz+3)&~3
    assert kind not in notes and pos+size<=end
    notes[kind]=data[pos:pos+size];pos+=(size+3)&~3
assert pos==end and set(notes)=={1,2,3}
info=struct.unpack('<IIiiHHHHq16s',notes[1])
assert info==(1,11,123,45,42,42,7,7,1700000000,b'TASK52'+bytes(10))
regs=struct.unpack('<22Q',notes[2])
assert regs[3]==0x123456789abcdef0 and regs[17]==0x40000040
assert len(notes[3])==512 and notes[3][160:176]==b'\x5a'*16
assert data[ph[1][2]:ph[1][2]+8192]==b'\x31'*4096+b'\x32'*4096
assert data[ph[2][2]:ph[2][2]+4096]==b'\x33'*4096
result=subprocess.run(['readelf','-h','-l','-n','build/core-fixture.elf'],capture_output=True,text=True,check=True)
assert 'CORE' in result.stdout and 'NEXIX' in result.stdout and not result.stderr
print('PASS: independent ELF64 core, vendor notes, user registers/FPU and data/stack inspection')

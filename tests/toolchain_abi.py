"""Inspect host-bootstrap ELF artifacts against the frozen native ABI."""
from pathlib import Path
import struct


def header(data, kind):
    if len(data) < 64 or data[:9] != b'\x7fELF\x02\x01\x01\x00\x00':
        raise ValueError('wrong ELF encoding')
    h = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    if h[0:3] != (kind, 62, 1) or h[6] or h[7] != 64:
        raise ValueError('wrong ELF type/machine/version')
    return h


data = Path('build/toolchain-abi.elf').read_bytes()
h = header(data, 2)
assert h[8] == 56 and h[9] > 0
loads = []
for i in range(h[9]):
    p = struct.unpack_from('<IIQQQQQQ', data, h[4] + i * 56)
    assert p[0] not in (2, 3, 7), 'dynamic/interpreter/TLS segment'
    if p[0] == 1:
        assert p[1] in (4, 5, 6) and p[5] <= p[6] and p[2] + p[5] <= len(data)
        assert p[7] == 4096 and p[3] % 4096 == p[2] % 4096
        loads.append(p)
    if p[0] == 0x6474e551:
        assert not p[1] & 1, 'executable stack'
assert any(p[1] == 5 and p[3] <= h[3] < p[3] + p[6] for p in loads)
assert min(p[3] for p in loads) == 0x40000000
assert any(p[1] == 6 and p[6] > p[5] for p in loads), 'missing zero-filled BSS'
seen = set()
for filename in ('toolchain-abi-caller.o', 'toolchain-abi-callee.o'):
    obj = Path('build', filename).read_bytes(); oh = header(obj, 1)
    assert oh[10] == 64
    for i in range(oh[11]):
        s = struct.unpack_from('<IIQQQQIIQQ', obj, oh[5] + i * 64)
        if s[1] == 4:
            assert s[9] == 24 and s[5] % 24 == 0
            for pos in range(s[4], s[4] + s[5], 24):
                reloc = struct.unpack_from('<QQq', obj, pos)[1] & 0xffffffff
                assert reloc in (1, 2, 4, 10, 11), ('unsupported bootstrap relocation', reloc)
                seen.add(reloc)
assert {2, 4}.issubset(seen), 'fixtures must exercise addresses and external calls'
for offset, replacement in ((0,b'BAD!'),(4,b'\x01'),(5,b'\x02'),(18,b'\x03\x00')):
    bad = bytearray(data); bad[offset:offset+len(replacement)] = replacement
    try:
        header(bad, 2)
    except ValueError:
        continue
    raise AssertionError('invalid encoding/machine accepted')
print('PASS: ELF64 AMD64 ET_REL/ET_EXEC, static RX/RW/BSS layout, relocations and rejected ABI drift')

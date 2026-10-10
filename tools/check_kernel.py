"""Check the ELF load contract and Multiboot2 header without external tools."""
import struct
import sys
from pathlib import Path


def check(path):
    data = Path(path).read_bytes()
    assert data[:7] == b"\x7fELF\x02\x01\x01", "expected little-endian ELF64"
    header = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
    kind, machine, _, entry, phoff, _, _, _, phsize, phcount, *_ = header
    assert kind == 2 and machine == 62, "expected x86_64 executable"
    loads = []
    for i in range(phcount):
        ptype, flags, offset, vaddr, paddr, size, memsize, align = struct.unpack_from(
            "<IIQQQQQQ", data, phoff + i * phsize)
        assert ptype != 3, "kernel must not use a dynamic interpreter"
        if ptype == 1:
            assert size <= memsize and offset + size <= len(data)
            assert vaddr == paddr and paddr + memsize <= 1 << 30
            assert not (flags & 2 and flags & 1), "writable executable segment"
            loads.append((vaddr, vaddr + memsize, flags, offset, size))
    assert any(lo <= entry < hi and flags & 1 for lo, hi, flags, _, _ in loads)
    found = False
    for offset in range(0, min(len(data), 32768) - 23, 8):
        magic, arch, length, checksum = struct.unpack_from("<IIII", data, offset)
        if magic != 0xe85250d6:
            continue
        assert arch == 0 and length >= 24 and length % 8 == 0
        assert offset + length <= min(len(data), 32768)
        assert (magic + arch + length + checksum) & 0xffffffff == 0
        assert struct.unpack_from("<HHI", data, offset + length - 8) == (0, 0, 8)
        assert any(start <= offset and offset + length <= start + size
                   for _, _, _, start, size in loads), "header must be loaded"
        found = True
        break
    assert found, "no aligned Multiboot2 header in first 32 KiB"
    print("PASS: ELF64 load layout and Multiboot2 header")


if __name__ == "__main__":
    try:
        check(sys.argv[1])
    except (AssertionError, IndexError, OSError, struct.error) as error:
        sys.exit(f"FAIL: {error}")

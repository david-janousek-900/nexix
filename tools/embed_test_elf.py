"""Embed an acceptance ELF without non-runtime symbols; retain its full build ELF.

Only private test executables use this path. Installed programs and nlist,
tracing and profiling fixtures retain their original symbol tables.
"""
import argparse
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def loads(data):
    if data[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('expected little-endian ELF64')
    header = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    records = []
    for i in range(header[9]):
        kind, flags, offset, va, pa, size, memory, align = struct.unpack_from(
            '<IIQQQQQQ', data, header[4] + i * header[8])
        if kind == 1:
            if size > memory or offset + size > len(data):
                raise ValueError('invalid load segment')
            records.append((flags, va, pa, size, memory, align, data[offset:offset+size]))
    if not records:
        raise ValueError('no load segments')
    return header[:4], records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('output')
    parser.add_argument('--ld', default='ld')
    parser.add_argument('--objcopy', default='objcopy')
    args = parser.parse_args()
    source, output = Path(args.source).resolve(), Path(args.output).resolve()
    prefix = '_binary_' + re.sub('[^a-zA-Z0-9_]', '_', args.source)
    with tempfile.TemporaryDirectory(prefix='nexix-test-elf-') as directory:
        folder = Path(directory)
        payload, raw = folder / 'payload', folder / 'raw.o'
        subprocess.run([args.objcopy, '--strip-all', str(source), str(payload)], check=True)
        if loads(source.read_bytes()) != loads(payload.read_bytes()):
            raise ValueError('stripping changed the executable load contract')
        subprocess.run([args.ld, '-r', '-b', 'binary', 'payload', '-o', str(raw)], cwd=folder, check=True)
        command = [args.objcopy, '--rename-section', '.data=.rodata,alloc,load,readonly,data,contents']
        for suffix in ('start', 'end', 'size'):
            command += ['--redefine-sym', f'_binary_payload_{suffix}={prefix}_{suffix}']
        subprocess.run(command + [str(raw), str(output)], check=True)


if __name__ == '__main__':
    main()

"""Pack a reproducible GRUB-readable gzip ELF to reduce boot-loader RAM use.

Keep the original ELF for layout validation, debugging and symbol lookup.
"""
import gzip
from pathlib import Path
import sys

if __name__ == '__main__':
    source, destination = map(Path, sys.argv[1:3])
    destination.write_bytes(gzip.compress(source.read_bytes(), mtime=0))

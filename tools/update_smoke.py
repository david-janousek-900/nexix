"""Check real periodic sync through the on-disk V7 superblock clock."""
from pathlib import Path
import shutil
import sys
import tempfile
import time
from guest_acceptance import Guest
from multiuser_acceptance import login

qemu, iso, ram = sys.argv[1:4]


def stamp(disk):
    with disk.open('rb') as stream:
        stream.seek(512 + 414)
        data = stream.read(4)
    # V7 longs store the high little-endian word before the low word.
    return (int.from_bytes(data[:2], 'little') << 16) | int.from_bytes(data[2:], 'little')


with tempfile.TemporaryDirectory(prefix='nx-update-') as temp:
    disk = Path(temp) / 'root.img'
    shutil.copyfile('build/distribution.img', disk)
    guest = Guest(qemu, iso, disk, ram, 2)
    try:
        login(guest, 'root', 'nexix')
        guest.command('/etc/update')
        guest.command("ps ax | grep -c 'update$'", stdout='1\n')
        # Wait for the child's immediate sync before selecting the baseline.
        end = time.monotonic() + 1
        while time.monotonic() < end: guest.pump(.1)
        before = stamp(disk)
        # No guest commands or explicit sync run during this interval.
        end = time.monotonic() + 33
        while time.monotonic() < end: guest.pump(.1)
        after = stamp(disk)
        if after < before + 29:
            raise AssertionError(f'periodic update did not publish clock: {before} -> {after}')
        guest.command('echo UPDATE_ALIVE', stdout='UPDATE_ALIVE\n')
        guest.command("ps ax | grep -c 'update$'", stdout='1\n')
    finally:
        for line, output in enumerate(guest.buffers):
            Path(f'build/update-{ram}.tty{line}.log').write_bytes(output)
        guest.close()
print(f'PASS: {ram} update daemon forks, survives real alarm and syncs the persistent superblock')

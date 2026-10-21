"""Installed mknod with real node I/O, encoding, permissions and cold boot."""
from pathlib import Path
import shutil
import sys
import tempfile
from guest_acceptance import Guest
from multiuser_acceptance import login

qemu, iso, ram = sys.argv[1:4]
usage = 'usage: mknod name b/c major minor\n'
with tempfile.TemporaryDirectory(prefix='nx-mknod-') as temp:
    disk = Path(temp) / 'root.img'
    shutil.copyfile('build/distribution.img', disk)
    for phase in range(2):
        guest = Guest(qemu, iso, disk, ram, 2)
        try:
            login(guest, 'root', 'nexix')
            if phase == 0:
                guest.command('umask 022')
                guest.command('/etc/mknod /tmp/nx-null c 6 2')
                guest.command('/etc/mknod /tmp/nx-block b 255 255')
                guest.command('/etc/mknod /tmp/nx-null c 6 2', stdout='mknod: File exists\n', status=1)
                for args in ('', '/tmp/x z 1 2', '/tmp/x char 1 2', '/tmp/x c 256 0',
                             '/tmp/x c 0 256', '/tmp/x c -1 0', '/tmp/x c 0 1x',
                             '/tmp/x c 99999999999999999999 0', "/tmp/x c '' 0"):
                    guest.command('/etc/mknod ' + args, stdout=usage, status=2)
                login(guest, 'bob', 'bob', 1, prompt='$ ')
                guest.command('/etc/mknod /tmp/bob-node c 6 2', 1,
                              stdout='mknod: Not owner\n', status=1, prompt=r'\$ ')
                guest.command('sync')
            guest.command('echo discarded > /tmp/nx-null')
            guest.command('cat /tmp/nx-null')
            guest.command("ls -l /tmp/nx-block | grep -c '255,255'", stdout='1\n')
            guest.command("ls -l /tmp/nx-null | grep -c '^crw-r--r--'", stdout='1\n')
        finally:
            for line, output in enumerate(guest.buffers):
                Path(f'build/mknod-{ram}-{phase}.tty{line}.log').write_bytes(output)
            guest.close()
print(f'PASS: {ram} mknod encoding, parsing, permissions, null I/O and cold-boot persistence')

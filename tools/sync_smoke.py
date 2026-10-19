"""Verify installed sync and a cold restart on a disposable production root."""
from pathlib import Path
import shutil
import sys
import tempfile
from guest_acceptance import Guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-sync-') as temp:
    disk=Path(temp)/'root.img';shutil.copyfile('build/distribution.img',disk)
    for phase in range(2):
        guest=Guest(qemu,iso,disk,ram,2)
        try:
            login(guest,'root','nexix')
            if not phase:
                guest.command('echo SYNC_PERSIST > /tmp/sync-data')
                guest.command('/bin/sync')
                guest.command('/bin/sync ignored')
                guest.command('/bin/sync | cat')
            else:guest.command('cat /tmp/sync-data',stdout='SYNC_PERSIST\n')
        finally:
            Path(f'build/sync-{ram}-{phase}.log').write_bytes(guest.buffers[0]);guest.close()
print(f'PASS: {ram} installed sync status/output and cold-boot persistent data')

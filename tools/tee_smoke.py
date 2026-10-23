"""Installed native V7 tee acceptance."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'tee',{},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('echo one | tee /tmp/a /tmp/b',stdout='one\n')
    guest.command('cat /tmp/a /tmp/b',stdout='one\none\n')
    guest.command('echo two | tee -a /tmp/a >/dev/null; cat /tmp/a',stdout='one\ntwo\n')
    guest.command('echo three | tee /tmp/nodir/x /tmp/c',stdout='tee: cannot open /tmp/nodir/x\nthree\n',status=1)
    guest.command('cat /tmp/c',stdout='three\n')
print(f'PASS: {ram} tee files, append, terminal output and open errors')

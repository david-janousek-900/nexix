"""Installed native V7 find acceptance on the guest filesystem."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
Path('build/find-data').write_bytes(b'd'*1500)
with private_guest(qemu,iso,ram,'find',{'/tmp/data':'find-data'},single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('cd /tmp; mkdir t t/a t/a/b t/e; mv data t/a/data; echo x > t/a/b/x.c; echo y > t/top; chmod 751 t/top; ln t/top t/a/link')
    guest.command('find t -print | sort',stdout='t\nt/a\nt/a/b\nt/a/b/x.c\nt/a/data\nt/a/link\nt/e\nt/top\n')
    guest.command("find t -name '*.c' -print",stdout='t/a/b/x.c\n')
    guest.command('find t -type d -print | sort',stdout='t\nt/a\nt/a/b\nt/e\n')
    guest.command('find t -size +2 -print',stdout='t/a/data\n')
    guest.command('find t -perm 751 -print | sort',stdout='t/a/link\nt/top\n')
    guest.command('find t -links 2 -type f -print | sort',stdout='t/a/link\nt/top\n')
    guest.command('find t -user root -name top -print',stdout='t/top\n')
    guest.command("find t '(' -name x.c -o -name top ')' -print | sort",stdout='t/a/b/x.c\nt/top\n')
    guest.command("find t ! -type d -name 't*' -print",stdout='t/top\n')
    guest.command("find t -name x.c -exec cat {} ';'",stdout='x\n')
    guest.command('find t/a/b/ -print',stdout='t/a/b/\nt/a/b/x.c\n')
    guest.command('cd /etc; find . -name passwd -print',stdout='./passwd\n')
    guest.command('cd /tmp; find / -name passwd -print | grep -c /etc/passwd',stdout='1\n')
    guest.command('find t/a -cpio /tmp/out.cpio; od -o /tmp/out.cpio | sed -n 1p | sed "s/ .*//"; od -o /tmp/out.cpio | sed -n 1p | sed "s/^[0-7]* //;s/ .*//"',stdout='0000000\n070707\n')
    guest.command('find t -bogus',stdout='find: bad option -bogus\n',status=1)
    guest.command('find /tmp/missing -print',stdout='find: bad status-- /tmp/missing\n')
    guest.command('find t',stdout='Usage: find path-list predicate-list\n',status=1)
print(f'PASS: {ram} find traversal, predicates, operators, -exec, root/relative starts, cpio and errors')

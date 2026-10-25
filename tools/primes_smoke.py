"""Installed native V7 prime generation, including maximum-range arithmetic."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'primes',{'/tmp/prefix':'primes-prefix.elf'},single_user=False) as g:
    login(g,'root','nexix')
    g.command('/tmp/prefix 1 10',stdout='2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n')
    g.command("echo '90' | /tmp/prefix --stdin 8",stdout='97\n101\n103\n107\n109\n113\n127\n131\n')
    g.command('primes 72057594037927931',stdout='72057594037927931\n')
    g.command('primes 72057594037927932')
    g.command('primes 72057594037927936',stdout='Ouch.\n',status=1)
    g.command('primes 0')
print(f'PASS: {ram} installed primes, bounded streaming/pipes/stdin and exact upper-domain primality')

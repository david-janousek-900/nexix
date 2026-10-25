"""Installed native equivalent of V7 factor.s on disposable guests."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'factor',{},single_user=False) as g:
    login(g,'root','nexix')
    for n,factors in [(1,[]),(12,[2,2,3]),(97,[97]),(1000036000099,[1000003,1000033]),(2**55,[2]*55)]:
        g.command('factor '+str(n),stdout='\n'+''.join(f'     {p}\n' for p in factors))
    g.command("echo '49' | factor",stdout='\n     7\n     7\n')
    g.command('factor 72057594037927936',stdout='Ouch.\n',status=1)
    g.command('factor 0')
print(f'PASS: {ram} installed factor, repeated factors, prime/semiprime, stdin/zero and exact 56-bit range')

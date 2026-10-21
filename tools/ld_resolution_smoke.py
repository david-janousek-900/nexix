"""Exercise the production linker resolver as root and an ordinary guest user."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/ldresolve':'ld-resolve-test.elf'}
for name in ('main.o','libchain.a','invalid.bad'):
    artifacts['/tmp/'+name]='fixtures-ld/'+name
with private_guest(qemu,iso,ram,'ld-resolve',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    expected=Path('build/fixtures-ld/expected').read_text()
    guest.command('/tmp/ldresolve /tmp/main.o /tmp/libchain.a',stdout=expected)
    guest.command('/tmp/ldresolve /tmp/libchain.a /tmp/main.o',stdout='ld: undefined symbol: a\n',status=1)
    guest.command('/tmp/ldresolve /tmp/invalid.bad',stdout='ld: /tmp/invalid.bad: invalid ELF64 relocatable object\n',status=1)
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/ldresolve /tmp/main.o /tmp/libchain.a',terminal=1,stdout=expected)
print(f'PASS: {ram} native ld resolution, archive search, errors and ordinary-user consumption')

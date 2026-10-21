"""Native cpp -> c0 -> c1 -> as -> ld compilation and execution of V7 C."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-c')
parts=(F/'parts.txt').read_text().split()
names=parts+['dialect','wc','echo','cat']
artifacts={f'/tmp/{n}.c':f'fixtures-c/{n}.c' for n in names}
artifacts.update({'/tmp/fixture.exp':'fixtures-c/fixture.exp','/tmp/dialect.exp':'fixtures-c/dialect.exp'})
expected=(F/'fixture.exp').read_text()
words=len(expected.split());lines=expected.count('\n');chars=len(expected)
def pipeline(guest,p,terminal):
    for n in names:
        guest.command(f'/lib/cpp /tmp/{n}.c /tmp/{p}{n}.i',terminal=terminal)
        guest.command(f'/lib/c0 /tmp/{p}{n}.i /tmp/{p}{n}.1 /tmp/{p}{n}.2',terminal=terminal)
        guest.command(f'/lib/c1 /tmp/{p}{n}.1 /tmp/{p}{n}.2 /tmp/{p}{n}.s',terminal=terminal)
        guest.command(f'as -o /tmp/{p}{n}.o /tmp/{p}{n}.s',terminal=terminal)
        if n not in parts:
            guest.command(f'ld -o /tmp/{p}{n} /lib/crt0.o /tmp/{p}{n}.o -lc',terminal=terminal)
    # Short ld -r groups keep commands within the terminal line limit.
    groups=[]
    for k in range(0,len(parts),5):
        groups.append(f'/tmp/{p}g{k}.o')
        guest.command(f'ld -r -o /tmp/{p}g{k}.o '+' '.join(f'/tmp/{p}{n}.o' for n in parts[k:k+5]),terminal=terminal)
    guest.command(f'ld -o /tmp/{p}fixture /lib/crt0.o '+' '.join(groups)+' -lc',terminal=terminal)
    guest.command(f'/tmp/{p}fixture > /tmp/{p}out',terminal=terminal)
    guest.command(f'cmp /tmp/{p}out /tmp/fixture.exp',terminal=terminal)
    guest.command(f'/tmp/{p}dialect > /tmp/{p}dout',terminal=terminal)
    guest.command(f'cmp /tmp/{p}dout /tmp/dialect.exp',terminal=terminal)
    guest.command(f'/tmp/{p}echo native v7 echo',terminal=terminal,stdout='native v7 echo\n')
    guest.command(f'/tmp/{p}wc /tmp/fixture.exp',terminal=terminal,stdout=f'{lines:7d}{words:7d} {chars:7d} /tmp/fixture.exp\n')
    guest.command(f'/tmp/{p}cat /tmp/dialect.exp',terminal=terminal,stdout=(F/'dialect.exp').read_text())
    guest.command(f'echo "main() {{ x = ; }}" > /tmp/{p}bad.c; /lib/c0 /tmp/{p}bad.c /tmp/{p}b1 /tmp/{p}b2 2> /tmp/{p}berr',terminal=terminal,status=1)
    guest.command(f'grep -c "Expression syntax" /tmp/{p}berr',terminal=terminal,stdout='1\n')
with private_guest(qemu,iso,ram,'c-compiler',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    pipeline(guest,'r',0)
    login(guest,'bob','bob',line=1,prompt='$ ')
    pipeline(guest,'u',1)
print(f'PASS: {ram} native cpp/c0/c1/as/ld V7 C fixture, dialect and supplied commands as root and ordinary user')

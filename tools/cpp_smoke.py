"""Installed cpp acceptance on disposable native roots."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-cpp')
(f/'headers.c').write_text(''.join('#include <'+str(p.relative_to('libc/include'))+'>\n' for p in sorted(Path('libc/include').rglob('*.h'))))
(f/'include.c').write_text('#include "outer.h"\nINCLUDED\n')
(f/'bad.c').write_text('#if 1/0\nno\n#endif\n')
artifacts={'/tmp/source.c':'fixtures-cpp/fixture.c','/tmp/expected':'fixtures-cpp/expected.txt',
           '/tmp/parser.c':'fixtures-cpp/parser.in','/tmp/parser.out':'fixtures-cpp/parser.out',
           '/tmp/headers.c':'fixtures-cpp/headers.c','/tmp/inner.h':'fixtures-cpp/inner.h',
           '/tmp/outer.h':'fixtures-cpp/outer.h','/tmp/include.c':'fixtures-cpp/include.c',
           '/tmp/bad.c':'fixtures-cpp/bad.c'}
with private_guest(qemu,iso,ram,'cpp',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/lib/cpp -P /tmp/source.c > /tmp/output.i')
    guest.command('cmp /tmp/output.i /tmp/expected')
    guest.command('/lib/cpp -P /tmp/parser.c > /tmp/parser.i')
    guest.command('cmp /tmp/parser.i /tmp/parser.out')
    guest.command('/lib/cpp -P /tmp/headers.c > /tmp/headers.i')
    guest.command('/lib/cpp -P /tmp/include.c > /tmp/include.i')
    guest.command("grep -c '^19$' /tmp/include.i",stdout='2\n')
    guest.command('/lib/cpp -P /tmp/source.c /tmp/position.i')
    guest.command('cmp /tmp/position.i /tmp/expected')
    guest.command('cat /tmp/source.c | /lib/cpp -P > /tmp/stdin.i')
    guest.command('cmp /tmp/stdin.i /tmp/expected')
    guest.command('/lib/cpp -P /tmp/bad.c > /tmp/bad.i',stdout='/tmp/bad.c: 1: invalid division in preprocessor if\n',status=1)
    guest.command('/lib/cpp -P /tmp/source.c /absent/out',stdout='/tmp/source.c: 0: Can\'t create /absent/out\n',status=8)
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/lib/cpp -P /tmp/source.c > /tmp/bob.i',terminal=1)
    guest.command('cmp /tmp/bob.i /tmp/expected',terminal=1)
print(f'PASS: {ram} installed cpp macros, headers, V7 yacc output, stdin/output files, failures and unprivileged use')

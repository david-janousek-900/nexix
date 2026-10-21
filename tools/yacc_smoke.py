"""Installed native LALR generation, compared with sanitizer-checked output."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-yacc');(f/'bad.y').write_text('%%\nstart: absent;\n')
artifacts={'/tmp/grammar.y':'fixtures-yacc/grammar.y','/tmp/expected.c':'fixtures-yacc/y.tab.c',
           '/tmp/expected.h':'fixtures-yacc/y.tab.h','/tmp/expected.o':'fixtures-yacc/y.output','/tmp/bad.y':'fixtures-yacc/bad.y'}
artifacts.update({'/tmp/runtime.y':'fixtures-yacc/runtime.y','/tmp/recovery.y':'fixtures-yacc/recovery.y',
                  '/tmp/runtime.c':'fixtures-yacc/runtime.c','/tmp/recovery.c':'fixtures-yacc/recovery.c',
                  '/tmp/parser':'yacc-runtime.elf','/tmp/recover':'yacc-recovery.elf'})
with private_guest(qemu,iso,ram,'yacc',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('cd /tmp')
    guest.command('yacc -dv grammar.y')
    for actual,expected in [('y.tab.c','expected.c'),('y.tab.h','expected.h'),('y.output','expected.o')]:
        guest.command('cmp '+actual+' '+expected)
    guest.command('yacc bad.y',stdout='\nfatal error: nonterminal absent not defined!, line 3\n',status=1)
    guest.command('yacc -Z grammar.y',stdout='\nfatal error: illegal option: Z, line 1\n',status=1)
    guest.command('yacc runtime.y')
    guest.command('cmp y.tab.c runtime.c')
    guest.command('/lib/cpp -P y.tab.c > /tmp/runtime.i')
    guest.command('/tmp/parser 0')
    guest.command('/tmp/parser 20')
    guest.command('/tmp/parser 200',status=1)
    guest.command('yacc recovery.y')
    guest.command('cmp y.tab.c recovery.c')
    guest.command('/tmp/recover')
    guest.command('mkdir /tmp/bobgen')
    guest.command('chmod 777 /tmp/bobgen')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('cd /tmp/bobgen',terminal=1)
    guest.command('cp /tmp/grammar.y grammar.y',terminal=1)
    guest.command('yacc -d grammar.y',terminal=1)
    guest.command('cmp y.tab.c /tmp/expected.c',terminal=1)
    guest.command('cmp y.tab.h /tmp/expected.h',terminal=1)
print(f'PASS: {ram} native yacc typed grammar, precedence, exact C/header/state outputs, errors and unprivileged generation')

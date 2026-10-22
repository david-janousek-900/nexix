"""Installed /bin/lint, /usr/lib/lint1 and /usr/lib/lint2 with the native description libraries."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-lint')
NAMES=('main.c','util.c','defs.h','clean.c','lib.c','uselib.c','extra.c')
art={f'/tmp/lint-{n}':f'fixtures-lint/{n}' for n in NAMES}
DEFAULT=(F/'expected-default').read_text()
LIB=(F/'expected-lib').read_text()
def run(g,d,t):
    c=lambda line,**k:g.command(line,terminal=t,**k)
    c(f'mkdir {d}; cd {d}')
    for n in NAMES: c(f'cp /tmp/lint-{n} {n}')
    c('lint main.c util.c',stdout=DEFAULT)
    c('lint clean.c',stdout='')
    c('lint -n lib.c uselib.c',stdout=LIB)
    c('lint -hu main.c util.c | grep -c "defined("',stdout='0\n',status=1)
    c('lint -v util.c | grep -c argument',stdout='0\n',status=1)
    c('lint -ab extra.c | grep -c "long assignment"',stdout='1\n')
    c('lint -p clean.c',stdout='')
    c('lint -lm clean.c',stdout='')
    c('mkdir sub; cp main.c util.c defs.h sub; lint sub/main.c sub/util.c | grep -c "sub/util.c(3)  ::  sub/main.c(5)"',stdout='1\n')
    c('lint -DLIMIT=0 -I/tmp -UX clean.c',stdout='')
    c('lint missing.c 2>&1 | grep -c "cat: can.t open missing.c"',stdout='1\n')
    c('ls /tmp | grep -c "^lint\\."',stdout='0\n',status=1)
    c('cd /')
with private_guest(qemu,iso,ram,'lint',art,terminals=2,single_user=False) as g:
    login(g,'root','nexix')
    run(g,'/tmp/r',0)
    login(g,'bob','bob',line=1,prompt='$ ')
    run(g,'/tmp/u',1)
print(f'PASS: {ram} installed lint driver, lint1/lint2 and native description libraries (-n/-p/-h/-u/-v/-a/-b/-l/-D/-I, subdirectories, failures) as root and ordinary user')

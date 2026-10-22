"""awk front-end generation with the installed yacc, lex, ed and proc, and native parse trees."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-awk')
programs=sorted(p.stem for p in F.glob('*.awk') if p.stem!='bad')
art={f'/tmp/{n}':f'fixtures-awk/src-{n}' for n in ('awk.g.y','awk.lx.l','tokenscript','token.c')}
art.update({f'/tmp/gen-{n}':f'gen-awk/{n}' for n in ('y.tab.c','y.tab.h','awk.lx.c','token.c','proctab.c')})
art.update({f'/tmp/{p}.awk':f'fixtures-awk/{p}.awk' for p in programs+['bad']})
art.update({f'/tmp/{p}.exp':f'fixtures-awk/{p}.exp' for p in programs})
art.update({'/tmp/awk-parse':'awk-parse.elf','/tmp/proc':'awk-proc.elf'})
def run(g,t,d):
    c=lambda line,**k:g.command(line,terminal=t,**k)
    c(f'mkdir {d}; cd {d}; cp /tmp/awk.g.y /tmp/awk.lx.l /tmp/tokenscript /tmp/token.c .')
    # The supplied makefile's generation steps.
    c('yacc -d awk.g.y; cmp y.tab.c /tmp/gen-y.tab.c; cmp y.tab.h /tmp/gen-y.tab.h',stdout='\nconflicts: 91 shift/reduce\n')
    c('lex awk.lx.l; cmp lex.yy.c /tmp/gen-awk.lx.c')
    c('ed - < tokenscript; rm temp; cmp token.c /tmp/gen-token.c')
    c('/tmp/proc > proctab.c; cmp proctab.c /tmp/gen-proctab.c')
    for p in programs:
        c(f'/tmp/awk-parse -f /tmp/{p}.awk > {p}.out; cmp {p}.out /tmp/{p}.exp')
    c('/tmp/awk-parse -f /tmp/bad.awk > out 2> err; echo $?; cat err',
      stdout='2\nawk: syntax error near line 1\nawk: bailing out near line 1\n')
    c('/tmp/awk-parse \'{ x = 1 }\'',stdout='(PROGRAM nil\n  (PASTAT nil\n    (ASSIGN var:x con:1)) nil)\n')
    c('cd /')
with private_guest(qemu,iso,ram,'awk',art,terminals=2,single_user=False) as g:
    login(g,'root','nexix')
    run(g,0,'/tmp/r')
    login(g,'bob','bob',line=1,prompt='$ ')
    run(g,1,'/tmp/u')
print(f'PASS: {ram} awk yacc/lex/tokenscript/proc regeneration with installed tools and native parse trees as root and ordinary user')

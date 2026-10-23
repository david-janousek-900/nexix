"""Generated lex/yacc programs compiled by the installed native cc and run in the guest."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
lexfix=['spec.l','input','expected','table.l','table-input','table-expected','libln.l','libln-input','libln-expected']
artifacts={'/tmp/'+n:'fixtures-lex/'+n for n in lexfix}
artifacts.update({'/tmp/'+n:'fixtures-lexyacc/'+n for n in ('calc.y','calc.l','calc.in','calc.exp')})
def run(guest,t):
    c=lambda line,**k:guest.command(line,terminal=t,**k)
    # Compressed, uncompressed and %T extended-byte scanners from the installed lex.
    c('lex spec.l; cc -o scan lex.yy.c')
    c('./scan < /tmp/input > out; cmp out /tmp/expected')
    c('(echo %a 6000; echo %o 20000; cat spec.l) > fspec.l; lex -f fspec.l > stats')
    if ram=='16M':
        # The 13,000-line uncompressed scanner needs about 2.5 MB of assembler
        # heap; a 16 MiB guest leaves a process about 1.3 MB.
        c('cc -o fscan lex.yy.c 2> err; echo $?; grep -c "out of memory" err; test -f fscan',stdout='1\n1\n',status=1)
    else:
        c('cc -o fscan lex.yy.c')
        c('./fscan < /tmp/input > out; cmp out /tmp/expected')
    c('lex table.l; grep -c "U(x) ((x)&0377)" lex.yy.c',stdout='1\n')
    c('cc -o tscan lex.yy.c; ./tscan < /tmp/table-input > out; cmp out /tmp/table-expected')
    c('lex libln.l; cc -o lscan lex.yy.c -lln')
    c('./lscan < /tmp/libln-input > out; cmp out /tmp/libln-expected')
    # A yacc parser using a lex scanner, its token header and libln's yywrap.
    c('yacc -d calc.y; lex calc.l; cc -O -o calc y.tab.c lex.yy.c -lln',stdout='y.tab.c:\nlex.yy.c:\n')
    c('./calc < /tmp/calc.in > out; cmp out /tmp/calc.exp')
    c('cd /')
with private_guest(qemu,iso,ram,'lexyacc',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('mkdir /tmp/r; cd /tmp/r; cp /tmp/spec.l /tmp/table.l /tmp/libln.l /tmp/calc.y /tmp/calc.l .')
    run(guest,0)
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('mkdir /tmp/u; cd /tmp/u; cp /tmp/spec.l /tmp/table.l /tmp/libln.l /tmp/calc.y /tmp/calc.l .',terminal=1)
    run(guest,1)
print(f'PASS: {ram} native cc compiles lex C/%T/libln scanners'+(', uncompressed' if ram!='16M' else ', bounded uncompressed failure')+' and a lex/yacc calculator as root and ordinary user')

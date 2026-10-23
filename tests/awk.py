"""awk front end: generated parser/scanner/tables and parse trees under ASan/UBSan."""
from pathlib import Path
import os
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'tests/awk';GEN=ROOT/'build/gen-awk'
OUT=ROOT/'build/fixtures-awk';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
# Strict C89 hides glibc's dprintf/asprintf, which awk defines for itself.
CC_HOST=['cc','-std=c89','-funsigned-char','-fcommon','-w','-g','-fsanitize=address,undefined','-fno-sanitize-recover=undefined']
subprocess.run(['make','build/gen-awk/y.tab.c','build/gen-awk/proctab.c','build/awk-parse.elf','build/awk-proc.elf','build/v7-awk.elf'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)

# Generated tables: tokenscript reproduces the supplied token list, and proc
# maps every token to its procedure and print name.
assert (GEN/'token.c').read_bytes()==(ROOT/'reference/v7/usr/src/cmd/awk/token.c').read_bytes()
assert (GEN/'awk.h').read_bytes()==(GEN/'y.tab.h').read_bytes()
proctab=(GEN/'proctab.c').read_text()
assert 'obj (*proctab[75])() = {' in proctab and 'char *printname[75] = {' in proctab
for entry in ['/*PROGRAM*/\tprogram,','/*FIRSTTOKEN*/\tnullproc,','/*SUBSTR*/\tsubstr,','/*FNCN*/\tfncn,',
              '/*PROGRAM*/\t"(null)",','/*INDIRECT*/\t"$(",','/*MODEQ*/\t" %= ",']:
    assert entry in proctab,entry
assert proctab.count('nullproc,')==75-48,proctab.count('nullproc,')

def run(cmd,**kw):
    r=subprocess.run(cmd,env=ENV,capture_output=True,timeout=30,**kw)
    assert r.returncode in (0,2) and b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(cmd[:3],r.stderr[-2000:])
    return r
with tempfile.TemporaryDirectory(prefix='nx-awk-') as temp:
    d=Path(temp);parse=d/'awk-parse'
    subprocess.run([*CC_HOST,'-I',str(GEN),'-I',str(ROOT/'user/awk'),str(ROOT/'tests/awk_parse.c'),
        *[str(GEN/n) for n in ('y.tab.c','awk.lx.c','token.c','proctab.c')],
        *[str(ROOT/'user/awk'/(n+'.c')) for n in ('b','tran','lib','run','parse')],'-lm','-o',str(parse)],check=True)
    def tree(program=None,path=None):
        return run([str(parse),*(['-f',str(path)] if path else [program])])
    # Hand-checked trees: precedence, statements, patterns, ranges, functions,
    # redirections, escapes and regular-expression DFAs.
    for f in sorted(SRC.glob('*.awk')):
        r=tree(path=f)
        if f.stem=='bad':
            assert r.returncode==2 and r.stderr==b'awk: syntax error near line 1\nawk: bailing out near line 1\n',r
            continue
        assert r.returncode==0 and r.stdout==f.with_suffix('.exp').read_bytes(),(f,r.stdout.decode())
        (OUT/f.name).write_bytes(f.read_bytes());(OUT/(f.stem+'.exp')).write_bytes(r.stdout)
    (OUT/'bad.awk').write_bytes((SRC/'bad.awk').read_bytes())
    for n in ('awk.g.y','awk.lx.l','tokenscript','token.c'):(OUT/('src-'+n)).write_bytes((ROOT/'user/awk'/n).read_bytes())
    # Command-line programs end at their terminator; comments, blank lines and
    # continued lines are skipped.
    r=tree('# c\n\n{ x = 1 \\\n + 2 }  # trailing\n')
    assert r.stdout==b'(PROGRAM nil\n  (PASTAT nil\n    (ASSIGN var:x\n      (=ADD con:1 con:2))) nil)\n',r.stdout
    errors=[('{ print $1 ','syntax error near line 1\nawk: illegal statement near line 1'),
            ('BEGIN { print "abc\n" }','newline in string near line 1\nawk: syntax error near line 2\nawk: illegal statement near line 2'),
            ('/a\n/','newline in regular expression near line 1'),
            ('{ print "'+'\\n'*200+'" }','string too long near line 1'),
            ('{ print "'+'x'*200+'" }','string too long near line 1'),
            ('/['+'a-z'*60+']/','string too long near line 1'),
            ('/'+'a'*300+'/','regular expression too long\n'),
            ('/['+'\\t'*300+']/','string too long near line 1'),
            ('/(a|b)*a'+'(a|b)'*8+'/','regular expression too long\n'),
            ('{ '+'v'*300+' = 1 }','syntax error near line 1')]
    for program,message in errors:
        r=tree(program)
        assert r.returncode==2 and r.stderr.startswith(('awk: '+message).encode()),(program[:40],r.stderr)
    # Bytes beyond lex's 7-bit tables are not program text: the scanner echoes
    # them (leaving // here) without reading past its tables.
    r=tree('/\xe9/ { print "\xfc" }')
    assert r.stdout.startswith('\xe9\xfc'.encode()) and r.returncode==2,r
    rng=random.Random(174)
    alphabet=list('{}()[]/$~!<>=+-*%;,\n\t "\\abcxyz019.^|&#')+['BEGIN','END','print','printf','if','else','while','for','in',
        'next','exit','break','continue','length','substr','split','index','sprintf','NF','NR','++','--','+=','>>']
    for _ in range(400):
        program=' '.join(rng.choice(alphabet) for _ in range(rng.randrange(1,60)))
        tree(program)
print('PASS: awk front end ASan/UBSan: tokenscript/proc tables, hand-checked trees and DFAs, diagnostics, limits and 400 random programs')

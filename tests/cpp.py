"""Sanitizer acceptance of the production V7 cpp scanner and native evaluator."""
from pathlib import Path
import os
import random
import subprocess
ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'build/fixtures-cpp'
FOLDER.mkdir(exist_ok=True,parents=True)
BINARY=ROOT/'build/host-cpp-test'
subprocess.run(['cc','-std=gnu89','-funsigned-char','-fcommon','-Wno-comment',
                '-Wno-implicit-int','-Wno-return-type','-fsanitize=address,undefined',
                '-fno-pie','-no-pie','-g','user/cpp/cpp.c','user/cpp/expr.c','-o',str(BINARY)],check=True,cwd=ROOT)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def run(source,*args):
    result=subprocess.run([str(BINARY),*args],input=source,capture_output=True,text=True,timeout=5,env=ENV,cwd=ROOT)
    assert result.returncode>=0 and 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result
    return result

def good(source,expected,*args):
    r=run(source,'-P',*args)
    assert r.returncode==0 and r.stdout.split()==expected.split(),r
    return r.stdout

SOURCE='''#define identifier_shared_prefix_one 42
#define identifier_shared_prefix_two 8
#define SUM(a,b) ((a)+(b))
#define QUOTE(x) "x"
#define ALIAS identifier_shared_prefix_one
#if SUM(1,2) != 3
wrong_function_macro
#endif
#if defined(__x86_64__) && defined(__nexix__) && defined(__LP64__) && defined(unix)
#if !defined(pdp11) && !defined(vax)
#if 0xff == 255 && 077 == 63 && (1 || (1/0)) && !(0 && (1/0))
#if (0 ? (1/0) : 5) == 5 && (1 ? 7 : (1/0)) == 7
#if (1UL << 63) > 0 && -1 < 0 && -1 > 1U && (~0UL >> 63) == 1
#if ('\\n' == 10) && ('\\141' == 97) && (1 | 2 ^ 3) == 1
int generated(void) { return SUM(ALIAS,identifier_shared_prefix_two); }
QUOTE(native)
#else
wrong
#endif
#endif
#endif
#endif
#endif
#endif
'''
output=good(SOURCE,'int generated(void) { return ((42)+(8)); } "native"')
(FOLDER/'fixture.c').write_text(SOURCE)
(FOLDER/'expected.txt').write_text(output)
good('#define F(x) x+x\nF F(3)\n','F 3+3')
good('#define F(x) x\nF','F')
good('#define F(x) x\n#if F == 0\nok\n#endif\n','ok')
good('#define F(x) x\n#if F(5)==5\nok\n#endif\n','ok')
good('#define NAME 6\nNAME\n#undef NAME\nNAME\n','6 NAME')
good('CLI\n','9','-DCLI=9')
good('CLI\n','CLI','-DCLI=9','-UCLI')
good('#line 50 "logical.c"\n__LINE__ __FILE__\n','50 "logical.c"')
good('a/* text */b\n','ab') # V7 traditional comment removal
assert '/* text */' in good('a/* text */b\n','a/* text */b','-C')
good('#define F 3\nF','3')
(FOLDER/'inner.h').write_text('#define INCLUDED 19\n')
(FOLDER/'outer.h').write_text('#include "inner.h"\nINCLUDED\n')
good('#include "outer.h"\n','19','-I'+str(FOLDER))
good('#include <inner.h>\nINCLUDED\n','19','-I'+str(FOLDER))
# Compare evaluated expressions independently with the host C preprocessor.
expressions=['0','1','0xff==255','0xabcdef==11259375','1|2^3','-7/3==-2','-7%3==-1',
             '0&&(1/0)','1||(1/0)','0?1/0:1','1?1:1/0','-1<1U','~0UL>0',
             '(1UL<<63)>>63','(1? -1:0U)>0', 'defined(unix)', 'defined(absent)']
for expr in expressions:
    src='#if '+expr+'\nyes\n#else\nno\n#endif\n'
    ours=run(src,'-P');oracle=subprocess.run(['cc','-E','-P','-traditional-cpp','-x','c','-'],input=src,text=True,capture_output=True,check=True)
    assert ours.returncode==0 and ours.stdout.split()==oracle.stdout.split(),(expr,ours,oracle)
# Every installed native public header must preprocess with its real dependencies.
for header in sorted((ROOT/'libc/include').rglob('*.h')):
    name=header.relative_to(ROOT/'libc/include')
    r=run('#include <'+str(name)+'>\n','-P','-Ilibc/include','-Iinclude')
    assert r.returncode==0,(name,r.stderr)
# Compile and execute emitted code; do not just compare scanner text.
r=run(SOURCE.replace('QUOTE(native)',''),' -P'.strip())
(FOLDER/'generated.c').write_text(r.stdout+'\nint main(void){return generated()!=50;}\n')
subprocess.run(['cc',str(FOLDER/'generated.c'),'-o',str(FOLDER/'generated')],check=True)
subprocess.run([str(FOLDER/'generated')],check=True)
for source in ['#if 1/0\nx\n#endif\n','#if 1<<64\nx\n#endif\n',
               '#if 0xgg\nx\n#endif\n','#if 09\nx\n#endif\n','#if 1 +\nx\n#endif\n',
               '#if defined(123)\nx\n#endif\n','#if 1\nx\n','#else\n','#endif\n',
               '#if 1\n#else\n#else\n#endif\n','/* unfinished','"unfinished',
               '#include "unfinished\n', '#include <unfinished\n', '#include "absent.h"\n','#define 123 nope\n','x'*10000,
               '#define F(a) a\nF('+('a'*10000)+')\n',
               '#define F '+('a '*600000)+'\n', '#define SELF SELF\nSELF\n',
               '#if '+('1?1:'*200)+'1\nx\n#endif\n',
               '#if '+('('*200)+'1'+(')'*200)+'\nx\n#endif\n']:
    r=run(source,'-P');assert r.returncode!=0,(source[:80],r)
for args in [('-Z',),tuple('-DX'+str(i) for i in range(22)),tuple('-UX'+str(i) for i in range(22)),tuple('-Id' for _ in range(12))]:
    assert run('',*args).returncode!=0,args
for source in ['#define F('+','.join('formal_'+str(i)+'_'*30 for i in range(25))+') x\n',
               ''.join('#define NAME'+str(i)+' 1\n' for i in range(4200))]:
    assert run(source,'-P').returncode!=0
r=run('hello\n','-P','/dev/stdin','/dev/full');assert r.returncode!=0,r
r=run('','-P','/tmp');assert r.returncode!=0,r
# Deterministic malformed input probes catch crashes and hangs independently of
# whether a particular byte sequence is valid traditional preprocessing input.
rng=random.Random(178)
for _ in range(150):
    run(''.join(rng.choice('abc012#()/*\\\n\"\' ,+-?:') for _ in range(rng.randrange(1,300))),'-P')
print('PASS: cpp sanitizer, macros, LP64 lazy expressions, all native headers, generated C and malformed input')
# Feed real V7 yacc-generated C through cpp, compile it, and execute its parser.
subprocess.run(['make','build/host-v7-yacc'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
(FOLDER/'parse.y').write_text('''%term NUMBER
%%
start: NUMBER '=' NUMBER { if ($1 != $3) return 1; };
%%
int yylex(void) { static int n; n++; yylval=7; return n==1||n==3?NUMBER:n==2?'=':0; }
int yyerror(char *s) { return 0; }
int main(void) { return yyparse(); }
''')
subprocess.run([str(ROOT/'build/host-v7-yacc'),'parse.y'],cwd=FOLDER,
               env=dict(os.environ,NX_YACCPAR=str(ROOT/'reference/v7/usr/lib/yaccpar')),check=True)
generated=(FOLDER/'y.tab.c').read_text()
r=run(generated,'-P');assert r.returncode==0,r
(FOLDER/'parser.c').write_text(r.stdout)
(FOLDER/'parser.in').write_text(generated)
(FOLDER/'parser.out').write_text(r.stdout)
subprocess.run(['cc','-std=gnu89','-w',str(FOLDER/'parser.c'),'-o',str(FOLDER/'parser')],check=True)
subprocess.run([str(FOLDER/'parser')],check=True)
print('PASS: cpp consumes, compiles and runs actual V7 yacc-generated parser output')

"""Production native yacc generator acceptance, including malformed grammars."""
from pathlib import Path
import os
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-yacc';OUT.mkdir(exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1',NX_YACCPAR=str(ROOT/'rootfs/usr/lib/yaccpar'))
GRAMMAR='''%{
#include <stdlib.h>
static int result;
%}
%union { long number; char *text; }
%token <number> NUMBER
%type <number> expr
%left '+' '-'
%left '*' '/'
%%
start: expr { result=$1; } ;
expr: NUMBER | expr '+' expr { $$=$1+$3; } | expr '*' expr { $$=$1*$3; }
    | '(' expr ')' { $$=$2; } ;
%%
static const char *input;
int yylex(void) { int n=0; while(*input==' ') input++; if(*input>='0'&&*input<='9'){while(*input>='0'&&*input<='9')n=n*10+*input++-'0';yylval.number=n;return NUMBER;} return *input?*input++:0; }
int yyerror(char *s){ return 0; }
int main(int argc,char **argv){ input=argc>1?argv[1]:""; if(yyparse())return 1;return result!=14; }
'''
with tempfile.TemporaryDirectory(prefix='nx-yacc-') as temp:
    folder=Path(temp);binary=folder/'generator'
    subprocess.run(['cc','-std=gnu89','-funsigned-char','-fcommon','-w','-O1','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',*[str(ROOT/'user/yacc'/('y'+str(i)+'.c')) for i in range(1,5)],'-o',str(binary)],check=True)
    def run(text,*args,ok=True,filename="grammar.y"):
        (folder/filename).write_text(text)
        r=subprocess.run([str(binary),*args,filename],cwd=folder,env=ENV,capture_output=True,timeout=10)
        assert r.returncode>=0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
        if ok:assert r.returncode==0,r
        return r
    run(GRAMMAR,'-dv')
    assert 'NUMBER' in (folder/'y.tab.h').read_text()
    assert 'states' in (folder/'y.output').read_text()
    for name in ('grammar.y','y.tab.c','y.tab.h','y.output'):(OUT/name).write_bytes((folder/name).read_bytes())
    subprocess.run(['cc','-std=gnu89','-w','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','y.tab.c','-o','parser'],cwd=folder,check=True)
    for text in ('2+3*4','(2+3)*2+4'):
        subprocess.run([str(folder/'parser'),text],check=True,env=ENV)
    assert subprocess.run([str(folder/'parser'),'2+*3'],env=ENV).returncode==1
    # Cross all 32-bit lookahead word boundaries using independent terminal names.
    tokens=' '.join('TOKEN'+str(i) for i in range(80))
    text='%token '+tokens+'\n%%\nstart: '+' | '.join('TOKEN'+str(i) for i in range(80))+' ;\n'
    run(text)
    for text in ['%%\nstart: absent;\n','%token '+('x'*500)+'\n%%\nstart: ;\n',
                 '%token NUMBER 99999999999999999999\n%%\nstart: NUMBER;\n',
                 '%token '+ ' '.join('T'+str(i) for i in range(140))+'\n%%\nstart: ;\n',
                 '%%\nstart: '+('A '*6000)+';\nA: ;\n',
                 '%%\nstart: "unterminated\n', '%union { int x; \n',
                 ''.join('%type <type'+str(i)+'> N'+str(i)+'\n' for i in range(70))+'%%\nstart: ;\n']:
        assert run(text,ok=False).returncode!=0
    run((ROOT/'reference/v7/usr/src/cmd/mip/cgram.y').read_text(),'-dv')
    assert run(GRAMMAR,'-Z',ok=False).returncode!=0
    (folder/'y.tab.c').unlink();(folder/'y.tab.c').mkdir()
    assert run(GRAMMAR,ok=False).returncode!=0
    (folder/'y.tab.c').rmdir();(folder/'y.tab.c').symlink_to('/dev/full')
    assert run(GRAMMAR,ok=False).returncode!=0
    (folder/'y.tab.c').unlink()
    runtime=r'''%{
#include <string.h>
static int errors;
%}
%union { long number; char *pointer; }
%token <pointer> VALUE
%type <pointer> item
%%
start: item { if(strcmp($1,"native-pointer"))return 2; } ;
item: VALUE { $$=$1; } | '(' item ')' { $$=$2; } ;
%%
static int remaining,phase;
int yylex(void){ if(remaining>0){remaining--;phase++;return '(';}if(phase>=0){phase=-phase-1;yylval.pointer="native-pointer";return VALUE;}if(++phase<0)return ')';return 0; }
int yyerror(char *s){ errors++;return 0; }
int main(int argc,char **argv){ int r;remaining=argc>1?atoi(argv[1]):0;r=yyparse();return r; }
'''
    run(runtime,'-d',filename='runtime.y')
    subprocess.run(['cc','-std=gnu89','-w','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','y.tab.c','-o','runtime'],cwd=folder,check=True)
    for depth in (0,1,20):subprocess.run([str(folder/'runtime'),str(depth)],check=True,env=ENV)
    r=subprocess.run([str(folder/'runtime'),'200'],env=ENV,capture_output=True)
    assert r.returncode==1 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
    (OUT/'runtime.c').write_bytes((folder/'y.tab.c').read_bytes())
    (OUT/'runtime.y').write_text(runtime)
    recovery=r'''%{
#include <stdio.h>
static int errors,recovered;
%}
%%
start: lines ;
lines: | lines 'a' ';' | lines error ';' { recovered++; yyerrok; } ;
%%
static const char *input;
int yylex(void){return *input?*input++:0;}
int yyerror(char *s){errors++;return 0;}
int main(void){int r;input="a;?;a;";r=yyparse();return r||errors!=1||recovered!=1;}
'''
    run(recovery,'-v',filename='recovery.y')
    (OUT/'recovery.out').write_bytes((folder/'y.output').read_bytes())
    (OUT/'recovery.c').write_bytes((folder/'y.tab.c').read_bytes())
    subprocess.run(['cc','-std=gnu89','-w','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','y.tab.c','-o','recovery'],cwd=folder,check=True)
    subprocess.run([str(folder/'recovery')],check=True,env=ENV)
    (OUT/'recovery.c').write_bytes((folder/'y.tab.c').read_bytes())
    (OUT/'recovery.y').write_text(recovery)
    rng=random.Random(169)
    for _ in range(100):run(''.join(rng.choice('abc:;|%{}<>\n\"\'0123') for _ in range(rng.randrange(1,200))),ok=False)
subprocess.run(['make','build/yacc-runtime.elf','build/yacc-recovery.elf'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
print('PASS: yacc production ASan/UBSan, typed/precedence parser execution, 80-terminal lookaheads, malformed grammars and limits')

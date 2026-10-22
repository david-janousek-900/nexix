"""Production native lex generator acceptance, including malformed specifications."""
from pathlib import Path
import os
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-lex';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1',NX_LEXLIB=str(ROOT/'rootfs/usr/lib/lex'))
SPEC=r'''%{
int words, numbers, calls;
%}
D	[0-9]
L	[a-zA-Z_]
%Start COMMENT STR
%%
	int depth;
<COMMENT>"*/"		{ BEGIN 0; printf("[comment]"); }
<COMMENT>.|\n		;
"/*"			BEGIN COMMENT;
<STR>\"			{ BEGIN 0; printf("[str:%s]", yytext); }
<STR>[^"\n]+		yymore();
\"			{ BEGIN STR; yymore(); }
{D}+"."{D}*([eE][-+]?{D}+)?	printf("[real:%s]", yytext);
{D}+			{ numbers++; printf("[int:%s]", yytext); }
x{2,3}			printf("[xs:%d]", yyleng);
{L}({L}|{D})*/[ \t]*"("	{ calls++; printf("[call:%s]", yytext); }
{L}({L}|{D})*		{ words++; ECHO; }
^"#".*			printf("[directive@%d]", yylineno);
end$			printf("[eol-end]");
"@"			{ int c = input(); printf("[at:%c]", c); }
"!"			{ unput('?'); }
"?"			printf("[q]");
[ \t]+			putchar(' ');
\n			ECHO;
.			printf("[%c]", yytext[0]);
%%
main() { yylex(); printf("words=%d numbers=%d calls=%d lines=%d\n", words, numbers, calls, yylineno); return 0; }
yywrap() { return 1; }
'''
INPUT='#define X\nf (1) g(2.5e3) h 42 xx xxx xxxx\n/* a\nb */ "quoted text" @z ! end\nend end;\n'
EXPECTED=('[directive@1]\n[call:f] [(][int:1][)] [call:g][(][real:2.5e3][)] h [int:42] [xs:2] [xs:3] xxxx\n'
          '[comment] [str:"quoted text"] [at:z] [q] [eol-end]\nend end[;]\n'
          'words=4 numbers=2 calls=2 lines=6\n')
def table_spec(size,plus):
    return '%T '+size+'\n1\tAa\n2\tBb\n3\t0123456789\n10\t\\n\n'+plus+'\t+\n%T\n'+r'''%%
(a|A)+		printf("<a%d>", yyleng);
[bB][0-9]+	printf("<b%d>", yyleng);
"+"		printf("<plus>");
\n		printf("<nl>\n");
%%
main() { yylex(); return 0; }
yywrap() { return 1; }
'''
# %T input bytes are the declared character codes, not the characters.
TABLE=table_spec('256','200');TABLE_INPUT=b'\1\1\1\2\3\3\310\2\3\n'
NARROW=table_spec('','100');NARROW_INPUT=b'\1\1\1\2\3\3\144\2\3\n'
TABLE_EXPECTED='<a3><b3><plus><b2><nl>\n'
RATFOR=r'''%%
[0-9]+		{ yylex = 1; return }
[a-z]+		{ yylex = 2; return }
'''
# libln supplies main, yywrap, yyreject, yyless and allprint/sprint.
LIBLN=r'''%{
int s, h;
%}
%%
she		{ s++; REJECT; }
he		{ h++; REJECT; }
abcd		{ yyless(2); printf("[ab]"); }
cd		printf("[cd]");
xyz		{ yyless(-1); yyless(9); printf("[%s]", yytext); }
"#"		printf("s=%d h=%d", s, h);
"@"		{ allprint('\t'); allprint(1); allprint(0300); sprint("x y"); }
.|\n		ECHO;
'''
LIBLN_INPUT='she#\nabcd xyz@\nshe he#\n'
LIBLN_EXPECTED='shes=1 h=1\n[ab][cd] [xyz]\\t\\1  \\300x\\\bby\nshe hes=2 h=3\n'
CC_HOST=['cc','-std=gnu89','-funsigned-char','-fcommon','-w','-g','-fsanitize=address,undefined']
with tempfile.TemporaryDirectory(prefix='nx-lex-') as temp:
    folder=Path(temp);binary=folder/'generator'
    subprocess.run([*CC_HOST,'-O1','-I',str(ROOT/'user/lex'),str(ROOT/'build/gen-lex/parser.c'),
        *[str(ROOT/'user/lex'/(n+'.c')) for n in ('lmain','sub1','sub2','header')],'-o',str(binary)],check=True)
    def run(text,*args,ok=True,filename='spec.l',stdin=None):
        if text is not None:(folder/filename).write_text(text)
        r=subprocess.run([str(binary),*args,*([filename] if filename else [])],cwd=folder,env=ENV,capture_output=True,timeout=20,input=stdin)
        assert r.returncode>=0 and b'AddressSanitizer' not in r.stderr+r.stdout and b'runtime error:' not in r.stderr+r.stdout,r
        if ok:assert r.returncode==0,r
        return r
    def scanner(source,name,text,expected):
        subprocess.run([*CC_HOST,*([source] if isinstance(source,str) else source),'-o',name],cwd=folder,check=True)
        r=subprocess.run([str(folder/name)],input=text if isinstance(text,bytes) else text.encode(),env=ENV,capture_output=True,timeout=10)
        assert r.returncode==0 and r.stdout.decode('latin-1')==expected,(name,r)
    # Default C generation, report and every table/compression variant.
    assert run(SPEC).stdout==b''
    (OUT/'spec.l').write_text(SPEC);(OUT/'lex.yy.c').write_bytes((folder/'lex.yy.c').read_bytes())
    (OUT/'input').write_text(INPUT);(OUT/'expected').write_text(EXPECTED)
    scanner('lex.yy.c','scanner',INPUT,EXPECTED)
    # A word longer than yytext splits at its capacity; bytes beyond the 7-bit
    # tables take the default action (ncform guards both).
    scanner('lex.yy.c','long',b'a'*300+b' \xe9\n',
            'a'*300+' \xe9\nwords=2 numbers=0 calls=0 lines=2\n')
    run(SPEC,'-n');assert run(SPEC,'-n').stdout==b''
    (OUT/'lex-n.yy.c').write_bytes((folder/'lex.yy.c').read_bytes())
    assert (folder/'lex.yy.c').read_bytes()==(OUT/'lex.yy.c').read_bytes()
    r=run(SPEC,'-f',ok=False);assert r.returncode!=0 and b'Too many transitions' in r.stdout,r
    run('%a 6000\n%o 20000\n'+SPEC,'-f');scanner('lex.yy.c','uncompressed',INPUT,EXPECTED)
    (OUT/'lex-f.yy.c').write_bytes((folder/'lex.yy.c').read_bytes())
    r=run(SPEC,'-tv');assert r.stdout==(OUT/'lex.yy.c').read_bytes() and b'packed char classes' in r.stderr,r
    r=run(SPEC,'-v');assert b'positions(%p)' in r.stdout and b'output slots(%o)' in r.stdout,r
    r=run(None,'-t',filename=None,stdin=SPEC.encode());assert r.stdout==(OUT/'lex.yy.c').read_bytes(),r
    (folder/'a.l').write_text(SPEC[:SPEC.index('%%\nmain')]);(folder/'b.l').write_text(SPEC[SPEC.index('%%\nmain'):])
    run(None,'a.l','b.l',filename=None);assert (folder/'lex.yy.c').read_bytes()==(OUT/'lex.yy.c').read_bytes()
    run('%e 3000\n%p 5000\n%n 600\n%k 1500\n%a 4000\n%o 6000\n'+SPEC)
    scanner('lex.yy.c','enlarged',INPUT,EXPECTED)
    # %T character tables; codes above 128 select the extended-byte driver.
    run(NARROW,filename='narrow.l');assert b'U(x) x\n' in (folder/'lex.yy.c').read_bytes()
    scanner('lex.yy.c','narrow',NARROW_INPUT,TABLE_EXPECTED)
    run(TABLE,filename='table.l');assert b'U(x) ((x)&0377)' in (folder/'lex.yy.c').read_bytes()
    (OUT/'table.l').write_text(TABLE);(OUT/'table.c').write_bytes((folder/'lex.yy.c').read_bytes())
    (OUT/'table-input').write_bytes(TABLE_INPUT);(OUT/'table-expected').write_text(TABLE_EXPECTED)
    scanner('lex.yy.c','table',TABLE_INPUT,TABLE_EXPECTED)
    assert run(table_spec('300','200'),ok=False).returncode!=0
    # libln runtime, linked as the library rather than the generated program.
    run(LIBLN,filename='libln.l');(OUT/'libln.l').write_text(LIBLN);(OUT/'libln.c').write_bytes((folder/'lex.yy.c').read_bytes())
    (OUT/'libln-input').write_text(LIBLN_INPUT);(OUT/'libln-expected').write_text(LIBLN_EXPECTED)
    lib=[str(ROOT/'user/lex/lib'/(n+'.c')) for n in ('allprint','main','reject','yyless','yywrap')]
    scanner(['lex.yy.c',*lib],'libln',LIBLN_INPUT,LIBLN_EXPECTED)
    # smallex is the same generator with the SMALL table limits.
    small=folder/'smallex'
    subprocess.run([*CC_HOST,'-DSMALL','-O1','-I',str(ROOT/'user/lex'),str(ROOT/'build/gen-lex/parser.c'),
        *[str(ROOT/'user/lex'/(n+'.c')) for n in ('lmain','sub1','sub2','header')],'-o',str(small)],check=True)
    (folder/'spec.l').write_text(SPEC)
    r=subprocess.run([str(small),'spec.l'],cwd=folder,env=ENV,capture_output=True,timeout=20)
    assert r.returncode==0 and (folder/'lex.yy.c').read_bytes()==(OUT/'lex.yy.c').read_bytes(),r
    r=subprocess.run([str(small),'-v','spec.l'],cwd=folder,env=ENV,capture_output=True,timeout=20)
    assert b'/600 nodes(%e)' in r.stdout and b'/300 (%n)' in r.stdout,r
    (folder/'big.l').write_text('%%\n'+'|'.join('w%d'%i for i in range(150))+'\tECHO;\n')
    assert run(None,filename='big.l').returncode==0
    r=subprocess.run([str(small),'big.l'],cwd=folder,env=ENV,capture_output=True,timeout=20)
    assert r.returncode!=0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
    # Ratfor generation uses nrform; Fortran execution is task 173.
    run(RATFOR,'-r',filename='ratfor.l');text=(folder/'lex.yy.r').read_text()
    assert 'integer function yylex' in text and 'blockdata' in text and 'function yylook' in text,text[:400]
    (OUT/'ratfor.l').write_text(RATFOR);(OUT/'lex.yy.r').write_text(text)
    # Diagnostics and limits fail cleanly.
    failures=['%%\n"unterminated\n','%%\n[abc\n','%{\nint x;\n','%n 3\n%%\n[a-z]+[0-9]+[a-z]+\tECHO;\n',
              '%p 5\n%%\nabcdefghij\tECHO;\n','%e 3\n%%\nabcdef\tECHO;\n','%%\n(a\tECHO;\n',
              '%%\na\t{ /* never closed\n','%%\n'+'|'.join('w%d'%i for i in range(3000))+'\tECHO;\n',
              '%a 3\n%%\n[a-z]+[0-9]+\tECHO;\n','%o 10\n%%\n[a-z]+[0-9]+\tECHO;\n']
    # V7 diagnoses these as warnings and still generates a scanner.
    for text,warning in [('%%\n{UNDEFINED}\tECHO;\n',b'Definition UNDEFINED not found'),
                         ('%%\n<NOSUCH>a\tECHO;\n',b'Undefined start condition NOSUCH'),('%x 3\n%%\na\t;\n',b'Invalid request %x 3')]:
        assert warning in run(text).stdout,text
    for text in failures:
        assert run(text,ok=False).returncode!=0,text
    assert run(None,ok=False,filename='absent.l').returncode!=0
    assert run(SPEC,'-Z').returncode==0
    (folder/'lex.yy.c').unlink();(folder/'lex.yy.c').mkdir()
    assert run(SPEC,ok=False).returncode!=0
    (folder/'lex.yy.c').rmdir()
    missing=dict(ENV,NX_LEXLIB=str(folder/'nowhere'))
    r=subprocess.run([str(binary),'spec.l'],cwd=folder,env=missing,capture_output=True)
    assert r.returncode!=0 and b'Lex driver missing' in r.stdout,r
    rng=random.Random(171)
    for _ in range(200):
        text=''.join(rng.choice('ab[]^-$.*+?|(){}<>"\\/%\n\t 01') for _ in range(rng.randrange(1,120)))
        run('%%\n'+text,ok=False)
        run(text,ok=False)
subprocess.run(['make','build/lex-scanner.elf','build/lex-table.elf','build/lex-libln.elf','build/libln.a'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
print('PASS: lex production ASan/UBSan, libln REJECT/yyless/allprint, smallex limits, C/uncompressed/%T scanner execution, Ratfor generation, options, malformed specifications and limits')

"""awk interpreter and data model under ASan/UBSan, with mawk as an oracle for POSIX-common cases."""
from pathlib import Path
import os
import random
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1];GEN=ROOT/'build/gen-awk'
OUT=ROOT/'build/fixtures-awkrun'
shutil.rmtree(OUT,ignore_errors=True);OUT.mkdir(parents=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
CC_HOST=['cc','-std=c89','-funsigned-char','-fcommon','-w','-g','-fsanitize=address,undefined','-fno-sanitize-recover=undefined']
PEOPLE='alice 10 x\nbob 20 y\ncarol 30 z\n\nalice 5 w\n'
REGEX=['/a/', '/^a/', '/a$/', '/^$/', '/^a.c$/', '/a\\.c/', '/a*b/', '/a+b/', '/ab?c/', '/(ab|ba)/', '/^(a|b)+$/', '/[abc]/', '/[^a-z]/', '/[a-c]+$/', '/[0-9]/', '/x[0-9]+/', '/\\t/', '/\\//', '/\\\\/', '/[\\]]/', '/a-b/', '/[-a]/', '/\\$/', '/\\^/', '/\\(/', '/\\+/', '/\\?/', '/^[A-Z]+$/', '/.../', '/^...$/', '/a|b|c/', '/^start.*end$/', '/o w/', '/mid/', '/^mid$/', '/[a-c][a-c][a-c]/', '/zz*y/', '/a(b|c)*/', '/^a*$/', '/(a*)*b/', '/x*/', '/^.$/']
REGEX_LINES=['','a','abc','aaa','ab','ba','x1','hello world','a.c','abc.','tab\there','^caret','dollar$','back\\slash','slash/x',']br','a-b','ABC','xyzzy','aXb','a+b','(paren)','q?','start mid end','mid','a b c','b','zy']
REGEX_PROGRAM=''.join(' { if ($0 ~ %s) printf "1"; else printf "0" }'%p for p in REGEX)+' { print "" }'
REGEX_INPUT='\n'.join(REGEX_LINES)+'\n'
# Expected rows were checked against mawk; each row is one input line.
# Arrays iterate in V7 hash order; these were checked as permutations.
WORDFREQ='a 1\nthe 2\ncat 2\ndog 1\n'
REPORT='name     amount\ntotal     65.00\ncarol     30.00\nbob       20.00\nalice     15.00\n'
REGEX_OUTPUT=(
    '000100000000000000000000000000000000001010\n'
    '111000000011010000000100000000100000011011\n'
    '110010111101010000000100000011100001010110\n'
    '111000000011010000000100000011100001011010\n'
    '110000110111010000000100000000100000010110\n'
    '101000100111010000000100000000100000010110\n'
    '000000000000101100000000000000000000000010\n'
    '000000000000100000000000000010001000000010\n'
    '110011000001110000000100000011100000010010\n'
    '110000111101100000000100000010100001010110\n'
    '100000110101100010000100000010100000010110\n'
    '100000000001100000000101000010100000010010\n'
    '100000000001100000000110000010100000010010\n'
    '100000101101100000100100000010100001010110\n'
    '100000000001100001000100000010100000010010\n'
    '000000100001100000010000000011100000000110\n'
    '110000100001110000001100000011100000010110\n'
    '000000000000100000000000000111000000000010\n'
    '000000000000000000000000000010000000100010\n'
    '110000100001110000000100000011100000010110\n'
    '110000100001110000000100010011100000010110\n'
    '100000000001100000000100100010100000010010\n'
    '000000000000100000000000001000000000000010\n'
    '100000000001100000000100000010110100010010\n'
    '000000000000000000000000000011000110000010\n'
    '110000100001110000000100000010100000010110\n'
    '000000100011010000000000000000100000000111\n'
    '000000000000000000000000000000000000100010\n'
)
# name, program, input, extra arguments, stdout, stderr, status, mawk agrees
CASES=[
 ('sums','{ s += $2; n[$1] += $2 } END { print s, NR; print n["alice"], n["bob"], n["carol"] }',PEOPLE,[],
  '65 5\n15 20 30\n','',0,True),
 ('fields','{ print NF, $NF, length($1), substr($1, 2, 3), index($1, "o") }',PEOPLE,[],
  '3 x 5 lic 0\n3 y 3 ob 2\n3 z 5 aro 4\n0  0  0\n3 w 5 lic 0\n','',0,True),
 ('compare','$2 > 15 { print $1 } $2 < 15 && $2 != "" { print "low", $1 }',PEOPLE,[],'low alice\nbob\ncarol\nlow alice\n','',0,True),
 ('range','NR == 2, NR == 3 { print NR ": " $0 }',PEOPLE,[],'2: bob 20 y\n3: carol 30 z\n','',0,True),
 ('rebuild','BEGIN { OFS = "-" } { $2 = $2 * 2; print; $1 = $1 }',PEOPLE,[],'alice-20-x\nbob-40-y\ncarol-60-z\n\nalice-10-w\n','',0,False),
 ('arith','BEGIN { print 7 % 3, -7 % 3, 1/3, 10/4, int(-2.5), int(3.9), 1e6, 1e30, 0.1+0.2, 3000000000, 100000*100000, -0.5 }','',[],
  '1 -1 0.333333 2.5 -2 3 1000000 1e+30 0.3 3000000000 10000000000 -0.5\n','',0,True),
 ('integers','BEGIN { x = 12345; print x, length(x), length(12345), -0, 1e18, -7, 2147483648 * 4 }','',[],'12345 5 5 -0 1000000000000000000 -7 8589934592\n','',0,False),
 ('assignops','BEGIN { x = 10; x += 5; x -= 3; x *= 2; x /= 4; x %= 4; print x; y = 5; print y++, y, ++y, y--, --y }','',[],
  '2\n5 6 7 7 5\n','',0,True),
 ('mathfns','BEGIN { printf "%.6f %.6f %.6f\\n", exp(1), log(10), sqrt(2); print length("hello"), length(12345) }','',[],
  '2.718282 2.302585 1.414214\n5 5\n','',0,True),
 ('split','{ n = split($0, a, ":"); print n, a[1], a[n] } END { m = split("p q  r", b); print m, b[3] }','one:two:three\nsolo\n',[],
  '3 one three\n1 solo solo\n3 r\n','',0,True),
 ('strings','{ s = s $1 } END { print s; s = s; print s; print substr(s, 4), substr(s, 0, 2), substr(s, 3, 100), substr("abc", 5) "|" }',PEOPLE,[],
  'alicebobcarolalice\nalicebobcarolalice\ncebobcarolalice al icebobcarolalice |\n','',0,False),
 ('control','{ while (i < 5) { i++; if (i == 2) continue; if (i == 4) break; print i }; for (j = 0; j < 3; j++) print "j" j; for (;;) break; exit } END { print "end" }','q\n',[],
  '1\n3\nj0\nj1\nj2\nend\n','',0,True),
 ('next','$1 == 2 { next } { print } END { print NR }','1\n2\n3\n',[],'1\n3\n3\n','',0,False),
 ('forin','{ c[$1]++ } END { for (k in c) n++; print n, c["alice"] }',PEOPLE,[],'4 2\n','',0,True),
 ('forsplit','{ split("a b", a); for (k in a) n += split("x y z", a); print n; for (k in a) if (a[k] == "y") print k }','q\n',[],'6\n2\n','',0,True),
 ('fornext','{ a["p"]; for (k in a) { if (k == "p") next; print k } } END { print "e" }','q\n',[],'e\n','',0,True),
 ('forself','{ a[1]; for (a in a) print }','q\n',[],'','awk: illegal reference to array a\n record number 1\n',2,False),
 ('indirect','{ i = 1; print $(i+1), $NF; $3 = ""; print }','a b c\n',[],'b c\na b \n','',0,True),
 ('fs','{ print $2, NF }','a:b:c\n',['-F:'],'b 3\n','',0,True),
 ('fstab','{ print $2 }','a\tb c\td\n',['-Ft'],'b c\n','',0,False),
 ('cmdvar','{ print v, NR }','x\n',['v=5','-'],'5 1\n','',0,True),
 ('paragraph','BEGIN { RS = "" } { print NR ": " $0 }','a\nb\n\nc\nd\n',[],'1: a\nb\n2: c\nd\n','',0,True),
 ('rs','BEGIN { RS = ";" } { print NR, $0 }','a;b;c',[],'1 a\n2 b\n3 c\n','',0,False),
 ('lastline','{ print NR, $0 }','x\ny',[],'1 x\n2 y\n','',0,True),
 ('constants','BEGIN { x = "1"; y = 1; z = "y"; print x + 0, y "", z }','',[],'1 1 y\n','',0,True),
 # V7: comparisons are numeric only when both values are numbers.
 ('v7compare','{ if ($1 < $2) print "num"; if ("10" < "9") print "str"; if (u < 1) print "u<1"; if (u == 0) print "u==0"; if (u == "") print "u==\\"\\"" }','10 9\n',[],
  'str\nu<1\nu==""\n','',0,False),
 # V7: $0 is empty in END, field assignment beyond NF leaves NF unchanged,
 # and OFMT applies to non-integers (invalid formats fall back to %.6g).
 ('v7end','END { print NR, "[" $0 "]" }',PEOPLE,[],'5 []\n','',0,False),
 ('v7nf','{ $5 = "e"; print; print NF; NF = 2; $1 = $1; print }','a b c\n',[],'a b c\n3\na b\n','',0,False),
 ('ofmt','BEGIN { OFMT = "%.2f"; print 1/3, 2; OFMT = "%s"; print 1/3; OFMT = "%d %g"; print 2/3 }','',[],'0.33 2\n0.333333\n0.666667\n','',0,False),
 ('temps','{ print substr($1 $2, 2, 3), index($1 $2, "b2"), length($1 $2) }','ab 2\n',[],'b2 2 3\n','',0,True),
 ('dividezero','BEGIN { print 1/0 }','',[],'','awk: division by zero\n',2,False),
 ('modzero','{ x %= 0 }','a\n',[],'','awk: division by zero\n record number 1\n',2,False),
 ('negfield','{ print $(-1) }','a\n',[],'','awk: trying to access field -1\n record number 1\n',2,False),
 ('bignf','{ NF = 80; $1 = "x"; print }','a\n',[],'','awk: trying to access field 80\n record number 1\n',2,False),
 ('longrec','{ print length }','x'*600+'\n',[],'',"awk: record `xxxxxxxxxxxxxxxxxxxx...' too long\n",2,False),
 ('manyfields','{ print NF }',' '.join('f'*1 for _ in range(60))+'\n',[],'',"awk: record `f f f f f f f f f f ...' has too many fields\n",2,False),
 ('longprint','{ s = $0 $0 $0 $0 $0 $0; print s, s }','y'*100+'\n',[],'','awk: string '+'y'*20+' ... too long to print\n record number 1\n',2,False),
 ('ranges',' '.join('NR==%d,NR==%d { }'%(i,i+1) for i in range(31)),'',[],'','awk: too many range patterns\n',2,False),
 ('syntax','{ print $1 ','',[],'','awk: syntax error near line 2\nawk: illegal statement near line 2\n',2,False),
 ('nofile','{ print }','',['/nonexistent'],'','awk: can\'t open /nonexistent\n',2,False),
 # Task 176: printf/sprintf, redirection, files, pipes and regular expressions.
 ('printf','BEGIN { printf "%d|%5d|%-5d|%05.1f|%e|%g|%s|%10s|%-3s|%x|%o|%%|%.2s\\n", 42.9, -3, 7, 3.14159, 12345.678, 0.0001, "str", "right", "l", 255, 8, "abcdef" }','',[],
  '42|   -3|7    |003.1|1.234568e+04|0.0001|str|     right|l  |ff|10|%|ab\n','',0,True),
 ('sprintf','BEGIN { s = sprintf("%d-%s%%", 5, "x"); print s, length(s); printf "%s\\n", "100%d"; printf "%5000d|\\n", 1 }','',[],
  '5-x% 4\n100%d\n'+' '*4999+'1|\n','',0,True),
 ('v7printf','BEGIN { printf "%c|%u|%*d|%d|%d\\n", 1e30, -1e30 }','',[],'%c|%u|%*d|9223372036854775807|-9223372036854775808\n','',0,False),
 ('printfwide','BEGIN { printf "%3000000000d", 1 }','',[],'','awk: format item %3000000000d... too long\n',2,False),
 ('printfargs','BEGIN { printf "%s %s\\n", "one" }','',[],'','awk: not enough arguments in printf(%s %s\n)\n',2,False),
 ('redirect','{ print $1 > "names"; print $2 >> "nums"; printf "[%s]\\n", $3 > "names" }',PEOPLE,[],'','',0,False,
  {'before':{'nums':'0\n'},'after':{'names':'alice\n[x]\nbob\n[y]\ncarol\n[z]\n\n[]\nalice\n[w]\n','nums':'0\n10\n20\n30\n\n5\n'}}),
 ('pipe','{ print $1 | "cat" } END { print "end" }',PEOPLE,[],'end\nalice\nbob\ncarol\n\nalice\n','',0,False),
 ('manyfiles','{ for (i = 0; i < 11; i++) print i > ("f" i) }','x\n',[],'','awk: too many output files 10\n record number 1\n',2,False),
 ('openfail','{ print > "/nonexistent/x" }','x\n',[],'','awk: can\'t open file /nonexistent/x\n record number 1\n',2,False),
 ('regex',REGEX_PROGRAM,REGEX_INPUT,[],REGEX_OUTPUT,'',0,True),
 # V7: "]" right after "[^" ends the class; write [^\]a] for "not ] or a".
 ('v7class','/[^\\]a]/ { n++ } /[^]a]/ { m++ } END { print n, m }','abc\n]]\naaa\nxa]\n',[],'2 1\n','',0,False),
 ('recover','{ if ($0 ~ /a/) printf "1" } { if ($0 ~ /o) printf "1" } { print }','',[],'','awk: syntax error near line 1\nawk: illegal statement near line 1\nawk: newline in regular expression near line 1\n',2,False),
 # Complete programs.
 ('wordfreq','{ for (i = 1; i <= NF; i++) n[$i]++ } END { for (w in n) printf "%s %d\\n", w, n[w] | "cat" }','the cat the dog\na cat\n',[],WORDFREQ,'',0,False),
 ('report','BEGIN { printf "%-8s %6s\\n", "name", "amount" } NF > 0 { t[$1] += $2; s += $2 } END { for (k in t) printf "%-8s %6.2f\\n", k, t[k] | "cat"; printf "%-8s %6.2f\\n", "total", s }',PEOPLE,[],
  REPORT,'',0,False),
 ('stats','NR == 1 { min = max = $1 } { s += $1; if ($1 < min) min = $1; if ($1 > max) max = $1 } END { printf "n=%d sum=%g mean=%.3f min=%g max=%g\\n", NR, s, s / NR, min, max }','5\n-2\n7.5\n10\n',[],
  'n=4 sum=20.5 mean=5.125 min=-2 max=10\n','',0,True),
 ('reverse','{ for (i = NF; i > 1; i--) printf "%s ", $i; print $1 }','a b c\n1 2\n',[],'c b a\n2 1\n','',0,True),
 ('number','{ printf "%4d  %s\\n", NR, $0 }','first\nsecond\n',[],'   1  first\n   2  second\n','',0,True),
 ('passwd','BEGIN { FS = ":" } $3 >= 100 { print $1, $6 > "users" } END { printf "%d accounts\\n", NR }','root:x:0:0::/:/bin/sh\nbob:x:100:10::/usr/bob:/bin/sh\nann:x:101:10::/usr/ann:\n',[],
  '3 accounts\n','',0,False,{'after':{'users':'bob /usr/bob\nann /usr/ann\n'}}),
]
subprocess.run(['make','build/gen-awk/y.tab.c','build/v7-awk.elf'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
with tempfile.TemporaryDirectory(prefix='nx-awkrun-') as temp:
    d=Path(temp);awk=d/'awk'
    subprocess.run([*CC_HOST,'-I',str(GEN),'-I',str(ROOT/'user/awk'),*[str(GEN/n) for n in ('y.tab.c','awk.lx.c','token.c','proctab.c')],
        *[str(ROOT/'user/awk'/(n+'.c')) for n in ('b','main','tran','lib','run','parse')],'-lm','-o',str(awk)],check=True)
    def run(cmd,data,**kw):
        r=subprocess.run(cmd,input=data.encode('latin-1'),capture_output=True,env=ENV,timeout=20,cwd=d,**kw)
        assert b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(cmd[:3],r.stderr[-2000:])
        return r
    names=[]
    for name,program,data,args,stdout,stderr,status,posix,*extra in CASES:
        files=extra[0] if extra else {}
        for f,text in files.get('before',{}).items():(d/f).write_text(text)
        (d/(name+'.awk')).write_text(program+'\n')
        # V7 stops reading options at -f: options first, then operands.
        opts=[a for a in args if a.startswith('-') and a!='-'];operands=[a for a in args if a not in opts]
        r=run([str(awk),*opts,'-f',name+'.awk',*operands],data)
        got=(r.stdout.decode('latin-1'),r.stderr.decode('latin-1'),r.returncode)
        assert got==(stdout,stderr,status),(name,got)
        for f,text in files.get('after',{}).items():
            assert (d/f).read_text()==text,(name,f,(d/f).read_text())
            (d/f).unlink()
        if posix:
            for f,text in files.get('before',{}).items():(d/f).write_text(text)
            m=subprocess.run(['mawk',*opts,'-f',name+'.awk',*operands],input=data.encode(),capture_output=True,cwd=d,timeout=20)
            assert m.stdout.decode()==stdout and m.returncode==0,(name,m)
        for suffix,text in (('.awk',program+'\n'),('.in',data),('.out',stdout),('.err',stderr),('.status',str(status)+'\n'),('.opts',' '.join(opts)+'\n'),('.operands',' '.join(operands)+'\n')):
            (OUT/(name+suffix)).write_bytes(text.encode('latin-1'))
        for kind in ('before','after'):
            for i,(f,text) in enumerate(sorted(files.get(kind,{}).items())):
                (OUT/f'{name}.{kind[0]}{i}').write_text(text);(OUT/f'{name}.{kind[0]}{i}.name').write_text(f+'\n')
        names.append(name)
    (OUT/'cases').write_text('\n'.join(names)+'\n')
    # Usage errors exit 2 before the symbol table exists.
    r=run([str(awk)],'');assert r.returncode==2 and r.stderr==b"awk: Usage: awk [-f source | 'cmds'] [files]\n",r
    r=run([str(awk),'-f'],'');assert r.returncode==2 and r.stderr==b'awk: no program file after -f\n',r
    # Random straight-line programs over random records must not fault.
    rng=random.Random(175)
    atoms=['$1','$2','$NF','$(NF-1)','x','y','a[$1]','a[x]','NR','NF','"s"','"12"','3','0.5','-2','length($1)','substr($0, 2, 3)',
           'index($0, "b")','int(x)','split($0, a, ",")','x y','(x)']
    ops=['+','-','*','/','%',' ']
    def expr(depth=0):
        if depth>2 or rng.random()<0.4:return rng.choice(atoms)
        return '('+expr(depth+1)+' '+rng.choice(ops)+' '+expr(depth+1)+')'
    for i in range(300):
        stmts=[]
        for _ in range(rng.randrange(1,6)):
            kind=rng.randrange(7)
            target=rng.choice(['x','y','a[$1]','$2','$1','NF','OFS','OFMT'])
            if kind==0:stmts.append(f'{target} = {expr()}')
            elif kind==1:stmts.append(f'{target} {rng.choice(["+=","-=","*="])} {expr()}')
            elif kind==2:stmts.append(f'print {expr()}, {expr()}')
            elif kind==3:stmts.append(f'if ({expr()} > {expr()}) print {expr()}')
            elif kind==4:stmts.append(f'for (k in a) print k, a[k]')
            elif kind==5:
                fmt=''.join(rng.choice(['%','%','d','s','g','f','e','x','o','c','u','l','*','-','0','5','.','3','99','a',' ','\\\\n']) for _ in range(rng.randrange(1,14)))
                stmts.append(f'printf "{fmt}", {expr()}, {expr()}, {expr()}')
            else:stmts.append(f'x = sprintf("%s|%d|%.3g", {expr()}, {expr()}, {expr()})'.replace('%s|',rng.choice(['%s|','%-9s|','%.2s|'])))
        program='{ '+'; '.join(stmts)+' } END { print x, y, NR }'
        lines=[' '.join(rng.choice(['a','bb','10','-3','2.5e1','','x,y','0']) for _ in range(rng.randrange(0,6))) for _ in range(rng.randrange(0,5))]
        r=run([str(awk),program],'\n'.join(lines)+'\n')
        assert r.returncode in (0,2),(program,r)
print(f'PASS: awk ASan/UBSan: {len(CASES)} data-model, arithmetic, field, array, string, control, printf, redirection, pipe, regex and complete-program cases ({sum(c[7] for c in CASES)} agreeing with mawk), V7 semantics, limits, errors and 300 random programs, including printf formats')

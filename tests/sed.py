"""sed: addresses, commands, hold space, branching, files and native bounds.

Scripts that avoid V7's unbounded paths are also compared with the V7 sources
built for the host; only its untagged member overlay is repaired so it compiles."""
import random
import subprocess
from v7_command import Command
c=Command('sed',sources=['user/v7/cmd/sed/sed0.c','user/v7/cmd/sed/sed1.c'])
refdir=c.dir/'ref';refdir.mkdir()
h=open('reference/v7/usr/src/cmd/sed/sed.h').read();new=open('user/v7/cmd/sed/sed.h').read()
h=h[:h.index('union	reptr {')]+new[new.index('union	reptr {'):new.index('} ptrspace[PTRSIZE], *rep;')]+h[h.index('} ptrspace[PTRSIZE], *rep;'):]
(refdir/'sed.h').write_text(h)
for name in ('sed0.c','sed1.c'):(refdir/name).write_text(open('reference/v7/usr/src/cmd/sed/'+name).read())
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char','-fcommon','-include','stdlib.h','-include','unistd.h','-include','fcntl.h',
                str(refdir/'sed0.c'),str(refdir/'sed1.c'),'-o',str(refdir/'sed')],check=True)
text='one\ntwo\nthree\nfour\nfive\n'
def same(args,data=text):
    want=subprocess.run([str(refdir/'sed'),*map(str,args)],input=data.encode(),capture_output=True,cwd=c.dir,timeout=10)
    return c.check(args,want.stdout,want.returncode,data=data)
c.check(['s/o/0/g'],'0ne\ntw0\nthree\nf0ur\nfive\n',data=text)
c.check(['-n','2,4p'],'two\nthree\nfour\n',data=text)
c.check(['-n','$p'],'five\n',data=text)
c.check(['/t/,/f/d'],'one\nfive\n',data=text)
c.check(['2!d'],'two\n',data=text)
c.check(['-n','/o/{s/o/O/;p;}'],'One\ntwO\nfOur\n',data=text)
c.check(['y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'],text.upper(),data=text)
c.check(['-n','='],'1\n2\n3\n4\n5\n',data=text)
c.check(['1!G;h;$!d'],'five\nfour\nthree\ntwo\none\n',data=text)
c.check(['$!N;P;D'],text,data=text)
c.check(['$!N;/^\\(.*\\)\\n\\1$/!P;D'],'a\nb\n',data='a\na\nb\nb\n')
c.check(['$!N;s/\\n/ /'],'one two\nthree four\nfive\n',data=text)
c.check(['-e',':a','-e','s/xy/y/','-e','ta'],'y\n',data='xxxxy\n')
c.check(['s/\\(o\\)\\(n\\)/\\2\\1/'],'noe\n',data='one\n')
c.check(['s/o/[&]/'],'[o]ne\n',data='one\n')
c.check(['3q'],'one\ntwo\nthree\n',data=text)
c.check(['2i\\\nINS\n4a\\\nAPP'],'one\nINS\ntwo\nthree\nfour\nAPP\nfive\n',data=text)
c.check(['2,3c\\\nCH'],'one\nCH\nfour\nfive\n',data=text)
r=c.file('rfile','R1\nR2\n')
c.check([f'1r {r}'],'one\nR1\nR2\ntwo\n',data='one\ntwo\n')
c.check([f'/two/w {c.dir/"wout"}'],'one\ntwo\n',data='one\ntwo\n'); assert (c.dir/'wout').read_text()=='two\n'
c.check([f's/one/ONE/w {c.dir/"sout"}'],'ONE\ntwo\n',data='one\ntwo\n'); assert (c.dir/'sout').read_text()=='ONE\n'
c.check(['-n','l'],'a\\01<\b-b\n',data='a\x01\x08b\n')
c.check(['-n','/x/!p'],'one\n',data='one\nx\n')
c.check(['s/[^a-z]/_/g'],'a_b_\n',data='a1b2\n')
f=c.file('script','#n\n/two/p\n')
c.check(['-f',f],'two\n',data=text)
c.check(['-e','s/a/b/','-e','s/b/c/'],'c\n',data='a\n')
c.check([],'',data=text)
c.check(['p',c.file('in1','x\n'),c.file('in2','y')],'x\nx\ny\ny\n')
c.check(['$s/$/!/',c.file('in3','x\n'),c.file('in4','y\n')],'x\ny!\n')
c.check(['s/y$/Y/'],'xY\n',data='xy')
c.check(['s/x*/-/g'],'-a-b-c\n',data='abc\n')
c.check(['s/b*/-/g'],'-a-c\n',data='abc\n')
c.check(['s/\\(*a\\)/[\\1]/'],'[*a]\n',data='*a\n')
c.check(['s/\\(\\)\\1*/-/'],'-abc\n',data='abc\n')
c.check([b's/[\xe9]/E/g'],b'cafE\n',data=b'caf\xe9\n')
c.check([b's/e/\xc3\xa9/'],b'\xc3\xa9\n',data=b'e\n')
c.check([b'y/\xe9/e/'],b'cafe\n',data=b'caf\xe9\n')
long='z'*9000
r=c.run(['s/z/zz/g'],data=long+'\n');assert r.returncode==0 and len(r.stdout)==8193 and r.stderr==b'Output line too long.\n',r
r=c.run(['G;G;G'],data=long+'\n');assert r.returncode==0 and len(r.stdout)==8193,r
r=c.run(['H;H;H;x'],data=long+'\n');assert r.returncode==0,r
r=c.run(['N;N;N'],data=(long+'\n')*4);assert r.returncode==0,r
c.check(['a\\\n'+'A'*20000],'',2,data='x\n',stderr='Command line too long\n')
c.check(['s/abc'],'',2,data='x\n',stderr='command garbled: s/abc\n')
c.check(['s/a/b'],'',2,data='x\n',stderr='command garbled: s/a/b\n')
c.check(['y/abc'],'',2,data='x\n',stderr='command garbled: y/abc\n')
c.check(['s/[a/b/'],'',2,data='x\n')
c.check(['b nowhere'],'',2,data='x\n',stderr='Undefined label: nowhere\n')
c.check(['{'*21],'',2,data='x\n')
c.check(['k'],'',2,data='x\n',stderr='Unrecognized command: k\n')
c.check(['-f'],'',2)
c.check(['-f',c.dir/'missing'],'',2,stderr=f"Cannot open pattern-file: {c.dir/'missing'}\n")
c.check(['p',c.dir/'missing'],'',2,stderr=f"Can't open {c.dir/'missing'}\n")
c.check(['$a\\\nA'],'x\nA\n',data='x\n')
r=c.run([':l\na\\\nA\ns/x//\nt l'],data='x'*150+'\n');assert r.returncode==0 and b'Too many appends' in r.stderr and r.stdout==b'\n'+b'A\n'*99,r
# Differential scripts over the bounded command set.
rng=random.Random(9)
cmds=['p','d','=','s/a/b/','s/a*/x/','s/\\(a\\)\\(b\\)/\\2\\1/g','s/[ab]/&&/','y/ab/ba/','h','H','g','G','x','N','n','P','$!N','/b/!d','2q','l']
for trial in range(600):
    script=';'.join(rng.choice(cmds) for _ in range(rng.randrange(1,4)))
    addr=rng.choice(['','1','2,3','/a/','/a/,/b/','$','2,$','1!'])
    parts=script.split(';')
    script=';'.join(([addr+parts[0]] if addr and not parts[0].startswith(('/','$','2q')) else [parts[0]])+parts[1:])
    data=''.join(''.join(rng.choice('ab c') for _ in range(rng.randrange(0,8)))+'\n' for _ in range(rng.randrange(1,7)))
    for flags in ([],['-n']):
        same(flags+[script],data)
print('PASS: sed ASan/UBSan addresses, commands, hold space, branches, files, 8-bit text, line bounds, errors and 1,200 reference comparisons')

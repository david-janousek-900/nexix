"""Installed PCC pass options and the native fort second pass with an independent producer."""
from pathlib import Path
import os
import re
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-pcc-driver';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
assert (ROOT/'rootfs/usr/lib/pcc.h').read_bytes()==(ROOT/'user/pcc/native.h').read_bytes(),'installed pcc.h differs from user/pcc/native.h'
M={m[0]:int(m[1]) for m in re.findall(r'#\s*define\s+(\w+)\s+(\d+)\s*$',(ROOT/'user/mip/manifest.h').read_text(),re.M)}
PTR,FTN,TSHIFT=0o40,0o100,2
def ptr(t): return ((t&~0o37)<<TSHIFT)|PTR|(t&0o37)
def ftn(t): return ((t&~0o37)<<TSHIFT)|FTN|(t&0o37)
LONG,DOUBLE=M['LONG'],M['DOUBLE']
FTEXT,FEXPR,FSWITCH,FLBRAC,FRBRAC,FEOF,FLABEL=200,201,202,203,204,205,207

class Stream:
    """An independent writer of the native fort intermediate format."""
    def __init__(self): self.b=bytearray()
    def word(self,w): self.b+=struct.pack('<Q',w&(2**64-1))
    def hdr(self,op,val=0,rest=0): self.word(op|val<<8|rest<<16)
    def words(self,data):
        data=data+b'\0'*(-len(data)%8)
        for k in range(0,len(data),8): self.b+=data[k:k+8]
    def name(self,s):
        self.word(len(s));self.words(s.encode())
    def text(self,s):
        data=s.encode()+b'\0'*(8-len(s.encode())%8 or 8)
        self.hdr(FTEXT,len(data)//8);self.words(data)
    def oreg(self,off,t,reg=0): self.hdr(M['OREG'],reg,t);self.word(off);self.name('')
    def icon(self,v,t,name=''):
        self.hdr(M['ICON'],1 if name else 0,t);self.word(v)
        if name: self.name(name)
    def op(self,o,t): self.hdr(o,0,t)
    def expr(self,line=1,file='f.f'):
        data=file.encode()+b'\0'*(8-len(file)%8 or 8)
        self.hdr(FEXPR,len(data)//8,line);self.words(data)
    def label(self,n): self.hdr(FLABEL,0,n)
    def goto(self,n): self.hdr(M['GOTO'],1,n)

def prologue(s,name,n,setup):
    s.text(f'.globl {name}\n.type {name},@function\n{name}:\n pushq %rbp\n movq %rsp,%rbp\n subq $.F{n},%rsp\n'+setup)
    s.hdr(FLBRAC,0,n);s.word(32)
def epilogue(s):
    s.text(' leave\n ret');s.hdr(FRBRAC)

def program():
    s=Stream()
    # fadd(a, b): a + b
    prologue(s,'fadd',1,' movq %rdi,-8(%rbp)\n movq %rsi,-16(%rbp)')
    s.oreg(-8,LONG);s.oreg(-16,LONG);s.op(M['PLUS'],LONG);s.op(M['FORCE'],LONG);s.expr()
    epilogue(s)
    # fsum(n): s = 0; for (i = 1; i <= n; i++) s += i
    prologue(s,'fsum',2,' movq %rdi,-8(%rbp)')
    s.oreg(-16,LONG);s.icon(0,LONG);s.op(M['ASSIGN'],LONG);s.expr(2)
    s.oreg(-24,LONG);s.icon(1,LONG);s.op(M['ASSIGN'],LONG);s.expr(3)
    s.label(1)
    s.oreg(-24,LONG);s.oreg(-8,LONG);s.op(M['LE'],M['INT']);s.icon(2,M['INT']);s.op(M['CBRANCH'],M['INT']);s.expr(4)
    s.oreg(-16,LONG);s.oreg(-16,LONG);s.oreg(-24,LONG);s.op(M['PLUS'],LONG);s.op(M['ASSIGN'],LONG);s.expr(5)
    s.oreg(-24,LONG);s.oreg(-24,LONG);s.icon(1,LONG);s.op(M['PLUS'],LONG);s.op(M['ASSIGN'],LONG);s.expr(6)
    s.goto(1)
    s.label(2)
    s.oreg(-16,LONG);s.op(M['FORCE'],LONG);s.expr(7)
    epilogue(s)
    # fcall(x): cfun(x + x, 3)
    prologue(s,'fcall',3,' movsd %xmm0,-8(%rbp)')
    s.icon(0,ptr(ftn(DOUBLE)),'cfun')
    s.oreg(-8,DOUBLE);s.oreg(-8,DOUBLE);s.op(M['PLUS'],DOUBLE)
    s.icon(3,LONG);s.op(M['CM'],LONG);s.op(M['CALL'],DOUBLE);s.op(M['FORCE'],DOUBLE);s.expr(8)
    epilogue(s)
    # freg(): *(frame - 8) = 42; return the automatic
    prologue(s,'freg',4,'')
    s.hdr(M['REG'],0,ptr(LONG));s.icon(-8,LONG);s.op(M['PLUS'],ptr(LONG));s.op(2+M['MUL'],LONG)
    s.icon(42,LONG);s.op(M['ASSIGN'],LONG);s.expr(9)
    s.oreg(-8,LONG);s.op(M['FORCE'],LONG);s.expr(10)
    epilogue(s)
    s.hdr(FEOF)
    return bytes(s.b)

DRIVER=r'''#include <stdio.h>
long fadd(long, long), fsum(long), freg(void); double fcall(double);
double cfun(double x, long n) { return x + n; }
#define CHECK(x) do { if (!(x)) { printf("FAIL fort line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
	CHECK(fadd(4000000000L, -3) == 3999999997L);
	CHECK(fsum(10) == 55 && fsum(0) == 0);
	CHECK(fcall(1.5) == 6.0);
	CHECK(freg() == 42);
	puts("PASS: native fort second pass");
	return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='nx-pcc-driver-') as tmp:
    d=Path(tmp);ccom=d/'ccom';fort=d/'fort'
    flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    mip=[]
    for name in ('common','trees','match','allo','reader'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','declarations','real','abi','code','local2','output']
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['user/pcc/ccom.c','build/gen-pcc/cgram.c',*mip,'-o',str(ccom)],cwd=ROOT,check=True)
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['user/pcc/fort.c','build/gen-pcc/cgram.c',*mip,'-o',str(fort)],cwd=ROOT,check=True)
    def run(cmd,**kw):
        r=subprocess.run(cmd,capture_output=True,env=ENV,timeout=20,**kw)
        assert b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(cmd,r.stderr)
        return r
    src=b'int g;\nint f(a) int a; { return a + g; }\nint h() { g = 3; return f(4); }\n'
    (d/'s.i').write_bytes(src)
    plain=run([str(ccom),str(d/'s.i')]);assert plain.returncode==0
    prof=run([str(ccom),'-p','--native-source',str(d/'s.i')]);assert prof.returncode==0
    assert prof.stdout.count(b'call mcount')==2 and b'call mcount' not in plain.stdout
    lines=run([str(ccom),'-l',str(d/'s.i'),str(d/'out.s')]);assert lines.returncode==0 and not lines.stdout
    assert b'# line 2' in (d/'out.s').read_bytes() and b'# line 3' in (d/'out.s').read_bytes()
    piped=run([str(ccom)],input=src);assert piped.stdout==plain.stdout
    debug=run([str(ccom),'-Xd',str(d/'s.i')]);assert debug.returncode==0
    emitted=run([str(ccom),'--emit-pass','-p','--native-source',str(d/'s.i')]);assert emitted.returncode==0
    split=run([str(ccom),'--read-pass'],input=emitted.stdout);assert split.stdout==prof.stdout
    for bad in (['-q'],['--emit-pass','--read-pass'],['a','b','c']):
        assert run([str(ccom),*bad]).returncode==2,bad
    assert run([str(ccom),str(d/'missing.i')]).returncode==1
    for o,name in ((d/'prof.s',prof.stdout),):
        o.write_bytes(name);subprocess.run(['as','--64',str(o),'-o',str(d/'prof.o')],check=True)
    # fort: an independent producer, GNU and native assembly, GCC driver.
    stream=program();(OUT/'fort.int').write_bytes(stream)
    r=run([str(fort),str(OUT/'fort.int')]);assert r.returncode==0,r.stderr
    (OUT/'fort.s').write_bytes(r.stdout)
    r2=run([str(fort)],input=stream);assert r2.stdout==r.stdout
    subprocess.run(['as','--64',str(OUT/'fort.s'),'-o',str(d/'fort.o')],check=True)
    (d/'driver.c').write_text(DRIVER);(OUT/'driver.c').write_text(DRIVER)
    subprocess.run(['cc','-no-pie',str(d/'driver.c'),str(d/'fort.o'),'-o',str(d/'fortprog')],check=True)
    r=subprocess.run([str(d/'fortprog')],capture_output=True,timeout=20)
    assert r.returncode==0 and r.stdout==b'PASS: native fort second pass\n',r.stdout
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables',
        '-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(d/'driver.c'),'-o',str(OUT/'fortdriver.o')],cwd=ROOT,check=True)
    # Malformed streams fail without memory errors.
    cases=[stream[:len(stream)//2],stream[:9],bytes(8),struct.pack('<Q',FSWITCH|1<<8)+bytes(8)+struct.pack('<Q',FEOF),
           struct.pack('<Q',M['PLUS']|M['INT']<<16)+struct.pack('<Q',FEOF),
           struct.pack('<Q',M['NAME']|LONG<<16)+struct.pack('<Q',300)+bytes(8)]
    for case in cases:
        r=run([str(fort)],input=case);assert r.returncode!=0,case[:16]
    # Programs for the installed driver in the guest.
    (OUT/'hello.c').write_text('#include <stdio.h>\n#include <string.h>\n#include "defs.h"\nstruct pt { int x; long y; };\nextern int twice();\nint main(void) { struct pt p; p.x = VAL; p.y = (long)strlen("native"); printf("hello %d %ld %d\\n", p.x, p.y, twice(21)); return 0; }\n')
    (OUT/'twice.c').write_text('int twice(int n) { return n * 2; }\n')
    (OUT/'defs.h').write_text('#define DEFAULT 1\n')
    (OUT/'asm.s').write_text('.text\n.globl twice\ntwice:\n movl %edi,%eax\n addl %edi,%eax\n ret\n.section .note.GNU-stack,"",@progbits\n')
    (OUT/'bad.c').write_text('int f( { return 1; }\n')
print('PASS: installed PCC pass options (-p mcount, -l line ids, -X, files, split), native fort second pass with an independent producer and GCC driver, malformed streams')

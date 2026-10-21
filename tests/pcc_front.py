"""Retained PCC semantic frontend: LP64 oracle, source parsing and shared mip IR."""
from pathlib import Path
import os
import random
import re
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-pcc';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
SOURCE='''#define STEP 5L
#if __LP64__
struct layout { char c; long value; short s; int n; double real; };
union shared { char c; long value; double real; };
struct layout object;
unsigned long size_char = sizeof(char);
unsigned long size_short = sizeof(short);
unsigned long size_int = sizeof(int);
unsigned long size_long = sizeof(long);
unsigned long size_pointer = sizeof(int *);
unsigned long size_struct = sizeof(struct layout);
unsigned long size_union = sizeof(union shared);
unsigned long size_array = sizeof(int[2][3]);
long offset_value = (long)&((struct layout *)0)->value;
long long_value = 4294967297L;
unsigned long unsigned_max = 0xffffffffffffffffUL;
int comparison = (-1L < 1U);
unsigned unsigned_wrap = 4294967295U + 1U;
int signed_char_value = (char)255;
unsigned long pointer_plus = (unsigned long)((long *)0x100000000UL+3);
long pointer_difference = (long *)0x100000100UL-(long *)0x100000000UL;
int pointer_compare = ((long *)0x100000000UL != 0);
int logical_constant = (1 && 2) || 0;
long combine(a,b) long a; unsigned b; { return a+b+STEP; }
long difference(a,b) long *a,*b; { return a-b; }
long *advance(p,n) long *p; int n; { return p+n; }
int control(a) int a; {
 int x;
 x=a;
 if(x>1) x=x-1;
 while(x<STEP) x++;
 switch(x){case 5: x=10; break; default: x=20;}
 return x;
}
struct layout copy_object(a,b) struct layout a,b; { a=b; return a; }
char message[]="native";
#endif
'''
(OUT/'types.c').write_text(SOURCE)
subprocess.run(['cc','-E','-P','-nostdinc',str(OUT/'types.c'),'-o',str(OUT/'types.i')],check=True)
with tempfile.TemporaryDirectory(prefix='nx-pcc-front-') as tmp:
    d=Path(tmp);binary=d/'front';flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','trace']
    mip=[]
    for name in ('common','trees'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['build/gen-pcc/cgram.c',*mip,'-o',str(binary)],cwd=ROOT,check=True)
    def run(text,ok=True):
        path=d/'input.i';path.write_text(text)
        p=subprocess.run([str(binary),str(path)],env=ENV,capture_output=True,timeout=5)
        assert p.returncode in (0,1) and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        assert (p.returncode==0)==ok,(text[:120],p)
        return p
    source=(OUT/'types.i').read_text();p=run(source);(OUT/'expected').write_bytes(p.stdout)
    trace=p.stdout.decode()
    # Host C independently proves the target size/offset and conversion values.
    oracle=d/'oracle.c';oracle.write_text(SOURCE+'\n#include <stdio.h>\nint main(void){printf("%lu %lu %lu %lu %lu %lu %lu %lu %ld %ld %lu %d %u %d %lu %ld %d %d\\n",size_char,size_short,size_int,size_long,size_pointer,size_struct,size_union,size_array,offset_value,long_value,unsigned_max,comparison,unsigned_wrap,signed_char_value,pointer_plus,pointer_difference,pointer_compare,logical_constant);return 0;}\n')
    executable=d/'oracle';subprocess.run(['cc','-w',str(oracle),'-o',str(executable)],check=True)
    values=subprocess.check_output([str(executable)]).decode().split()
    names=['size_char','size_short','size_int','size_long','size_pointer','size_struct','size_union','size_array','offset_value','long_value','unsigned_max','comparison','unsigned_wrap','signed_char_value','pointer_plus','pointer_difference','pointer_compare','logical_constant']
    for n,value in zip(names,values):
        part=trace.split('object '+n+' ')[1].split('node 0 ICON ')[1].splitlines()[0]
        actual=int(re.search(r'value=(-?\d+)',part)[1]) & ((1<<64)-1)
        assert actual==int(value)&((1<<64)-1),(n,actual,value)
    assert 'common object type=8 size=32 align=8' in trace
    assert re.search(r'node \d+ / type=5 ',trace),trace
    assert 'CBRANCH' in trace and 'STASG' in trace and 'STCALL' not in trace
    # Long names and all supported type modifiers retain native widths.
    longname='identifier_'+('x'*240)
    assert longname in run('long '+longname+';').stdout.decode()
    run('long '+('x'*256)+';',False)
    run('long '+('*'*30)+'pointer;')
    run('long '+('*'*31)+'pointer;',False)
    for text in ['int x=1/0;','int x=1<<64;','int x=1<<-1;','long x=(-9223372036854775807L-1L)/-1L;',
        'int x=18446744073709551616UL;','int x=0x;','int x=08;',
        'int f(){return "unterminated;}','int f(){return (((1);}',
        'struct a { struct a x; };','int x[999999999][999999999][999999999];',
        '# 214748364999 "a"\nint x;','# 1 "'+('x'*1100)+'"\nint x;']:
        run(text,False)
    run('# 40 "logical.c"\nint x;')
    run('# 2147483647 "logical.c"\nint x;\n',False)
    run('typedef long native_long;struct a {native_long same;};struct b {char same;};struct a av;struct b bv;long f(){return av.same+bv.same;}')
    run('int tag;struct tag {int field;};struct tag value;')
    run('struct bad {int duplicate;int duplicate;};',False)
    run('union bad {int duplicate;int duplicate;};',False)
    # Deterministic mutations check parser/error paths, not only valid programs.
    rng=random.Random(193)
    for _ in range(150):
        b=list(source)
        for _ in range(rng.randrange(1,6)):b[rng.randrange(len(b))]=rng.choice('ab01{}[]();,*+-/"\n ')
        path=d/'input.i';path.write_text(''.join(b))
        p=subprocess.run([str(binary),str(path)],env=ENV,capture_output=True,timeout=5)
        assert p.returncode in (0,1) and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
print('PASS: retained PCC frontend LP64/GCC oracle, declarations/control/aggregate IR, integer conversions, type depth, malformed-source sanitizers')

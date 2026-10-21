"""Retained PCC semantic frontend: LP64 oracle, source parsing and shared mip IR."""
from pathlib import Path
import os
import json
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
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','declarations','real','trace']
    mip=[]
    for name in ('common','trees'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['build/gen-pcc/cgram.c',*mip,'-o',str(binary)],cwd=ROOT,check=True)
    def run(text,ok=True,native=False):
        path=d/'input.i';path.write_text(text)
        p=subprocess.run([str(binary)]+(['--native-source'] if native else [])+[str(path)],env=ENV,capture_output=True,timeout=5)
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
    run('long '+('*'*29)+'pointer;')
    run('long '+('*'*30)+'pointer;',False)
    for text in ['int x=1/0;','int x=1<<64;','int x=1<<-1;','long x=(-9223372036854775807L-1L)/-1L;',
        'int x=18446744073709551616UL;','int x=0x;','int x=08;','long long x=1lL;','long long x=1lUl;',
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
    run('int add(int a,int b){return a+b;} int use(void){return add(2,3);}')
    run('int f(int);int f(double);',False)
    run('int f(float);int f();',False)
    run('int f(float);int f(x)float x;{return (int)x;}',False)
    run('int f(int);int f(x)char x;{return x;}')
    run('int f(int);int g(void){return f();}',False)
    run('int f(void);int g(void){return f(1);}',False)
    run('typedef long native; native f(native n){return n;}',native=True)
    run('typedef int arg;int old(arg)int arg;{return arg;}')
    run('int f(int (*callback)(int),int x){return callback(x);}')
    run('unsigned long a=sizeof(long long);unsigned long b=sizeof(long double);_Bool bval=7;')
    # The explicitly selected V7/native source profile adapts installed header
    # hints and spelling, while real declarations/prototypes use this parser.
    profile=ROOT/'user/pcc/native.h'
    (OUT/'native.h').write_bytes(profile.read_bytes())
    headers=[]
    for header in sorted((ROOT/'libc/include').rglob('*.h')):
        relative=header.relative_to(ROOT/'libc/include')
        cpp=subprocess.run(['cc','-E','-P','-nostdinc','-Ilibc/include','-Iinclude','-include',str(profile),'-x','c','-'],input=f'#include <{relative}>\n'.encode(),cwd=ROOT,capture_output=True,check=True)
        try:header_trace=run(cpp.stdout.decode(),native=True).stdout
        except AssertionError as error:raise AssertionError(str(relative)) from error
        name='header'+str(len(headers));headers.append(name)
        (OUT/(name+'.c')).write_text('#include "/tmp/native.h"\n'+f'#include <{relative}>\n')
        (OUT/(name+'.exp')).write_bytes(header_trace)
    (OUT/'headers.json').write_text(json.dumps(headers))
    run('void f(void){return;}void g(void){f();(void)3;}')
    run('int f(int,...);int g(void){return f(1,2.5);}')
    cpp=subprocess.run(['cc','-E','-P','-nostdinc','-include',str(profile),'-x','c','-'],input=b'long sum(int n,...){__nx_pcc_va_list ap,copy;long v;__builtin_va_start(ap,n);__builtin_va_copy(copy,ap);v=__builtin_va_arg(copy,long);__builtin_va_end(ap);return v;}',capture_output=True,check=True)
    run(cpp.stdout.decode())
    badva='long f(int n,...){__nx_pcc_va_list ap;{int n;__builtin_va_start(ap,n);}return 0;}'
    cpp=subprocess.run(['cc','-E','-P','-nostdinc','-include',str(profile),'-x','c','-'],input=badva.encode(),capture_output=True,check=True)
    run(cpp.stdout.decode(),False)
    native_source='#include "/tmp/native.h"\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdarg.h>\n#include <signal.h>\n#include <ctype.h>\n#include <sys/stat.h>\n#include <setjmp.h>\n#include <elf.h>\n#include <nexix/fpu.h>\nstruct native_layout {char a;long long b;long double c;_Bool d;};\nstruct native_layout native_object;\nunsigned long native_ll=sizeof(long long);\nunsigned long native_ld=sizeof(long double);\nunsigned long native_bool=sizeof(_Bool);\nunsigned long native_max=sizeof(max_align_t);\n_Bool native_truth=256;\nlong long native_rank=(long long)-1 < (unsigned long)1;\nlong double native_exact=0x1.0000000000000002p0L;\nint typed(int a,float b){return a+(int)b;}\nint callback(int (*fn)(int,float)){return fn(2,3.5);}\nlong inspect(int n,...){va_list ap,copy;long v;va_start(ap,n);va_copy(copy,ap);v=va_arg(copy,long);va_end(copy);va_end(ap);return v;}\nvoid native_void(void){return;}\nint native_calls(void){int n;int *p;p=&n;n=-3;native_void();return typed(n,1.25f);}\n_Static_assert(sizeof(long long)==8,"long long");\n_Static_assert(sizeof(long double)==16,"long double");\n_Static_assert(sizeof(max_align_t)==32,"max_align_t");\n_Static_assert(sizeof(struct native_layout)==48,"layout");\n'
    native_source=native_source.replace('#include <elf.h>\n','#include <elf.h>\n#include <math.h>\n')
    native_source+='double native_inf=INFINITY;double native_nan=NAN;int native_predicate=isnan(NAN)+2*isfinite(1.0)+4*signbit(-0.0);\n'
    (OUT/'native.c').write_text(native_source)
    cpp=subprocess.run(['cc','-E','-P','-nostdinc','-Ilibc/include','-Iinclude','-x','c','-'],input=native_source.replace('/tmp/native.h',str(profile)).encode(),cwd=ROOT,capture_output=True,check=True)
    native_trace=run(cpp.stdout.decode(),native=True).stdout
    (OUT/'native.exp').write_bytes(native_trace)
    assert b'extended-bits 8000000000000001 3fff' in native_trace
    run('int bad(int a,int a);',False)
    run('int f(int (*)(int));int g(double);int h(void){return f(g);}',False)
    run('struct __attribute_bad {int n;} __attribute__((aligned(3)));',False)
    run('void x;',False)
    run('int f(void){return (void)1;}',False)
    run('void f(void){return (void)1;}')
    badassert=subprocess.run(['cc','-E','-P','-nostdinc','-include',str(profile),'-x','c','-'],input=b'_Static_assert(sizeof(long)==4,"wrong");',capture_output=True,check=True)
    run(badassert.stdout.decode(),False)
    # GCC supplies an independent binary80 literal oracle, including exact
    # halfway ties, subnormals and precision beyond a binary64 significand.
    literals=['0.1L','1.0000000000000000001L','0x1.0000000000000001p0L','0x1.0000000000000002p0L','0x1.fffffffffffffffep0L','0x1p-16445L','0x1p16383L','1234567890123456789.0L','1e-4900L','1e4900L']
    declarations='\n'.join(f'long double literal{i}={value};' for i,value in enumerate(literals))
    results=run(declarations).stdout.decode()
    (OUT/'real.c').write_text(declarations)
    (OUT/'real.exp').write_text(results)
    oracle=d/'real-oracle.c';oracle.write_text('#include <stdio.h>\n#include <stdint.h>\n#include <string.h>\n'+declarations+'\nint main(void){\n'+''.join(f'{{uint64_t lo;unsigned short hi;memcpy(&lo,&literal{i},8);memcpy(&hi,(char *)&literal{i}+8,2);printf("%016lx %04x\\n",(unsigned long)lo,(unsigned)hi);}}\n' for i in range(len(literals)))+'return 0;}\n')
    subprocess.run(['cc',str(oracle),'-o',str(d/'real-oracle')],check=True)
    expected_real=subprocess.check_output([str(d/'real-oracle')]).decode().splitlines()
    assert re.findall(r'extended-bits (.*)',results)==expected_real,(results,expected_real)
    # Deterministic mutations check parser/error paths, not only valid programs.
    rng=random.Random(193)
    for _ in range(150):
        b=list(source)
        for _ in range(rng.randrange(1,6)):b[rng.randrange(len(b))]=rng.choice('ab01{}[]();,*+-/"\n ')
        path=d/'input.i';path.write_text(''.join(b))
        p=subprocess.run([str(binary),str(path)],env=ENV,capture_output=True,timeout=5)
        assert p.returncode in (0,1) and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
print('PASS: retained PCC frontend LP64/GCC oracle, declarations/control/aggregate IR, integer conversions, type depth, malformed-source sanitizers')

"""PCC integer backend: mixed GNU callers, native C compilation and executable oracles."""
from pathlib import Path
import os
import json
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-pcc';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
FUNCTIONS='''long global_value = 4294967297L;
int zeros[8];
long *global_pointer = &global_value;
char *greeting = "native";
long calc(a,b) long a; unsigned b; { long x; x=a+b; x*=3; return x+global_value; }
unsigned long huge_address(){return (unsigned long)&global_value+0x100000000UL;}
long indirect(){return *global_pointer;}
int literal(){return greeting[0]+greeting[5];}
unsigned wrap32(a,b) unsigned a,b;{return a+b;}
long cast_assignment(p) unsigned *p;{return (long)(int)(*p=0xffffffffU);}
long unary(a) long a;{return ~(-a);}
int flow(n) int n;{int i;int sum;sum=0;for(i=0;i<n;i++){if(i==2)continue;if(i==5)break;sum+=i;}return sum;}
int no_default(a) int a;{int v;v=6;switch(a){case 3:v=9;}return v;}
long many(a,b,c,d,e,f,g,h) long a,b,c,d,e,f,g,h; {return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h;}
long signed_divide(a,b) long a,b; {return a/b;}
long signed_remainder(a,b) long a,b; {return a%b;}
unsigned long unsigned_divide(a,b) unsigned long a,b; {return a/b;}
unsigned long unsigned_remainder(a,b) unsigned long a,b; {return a%b;}
long signed_shift(a,n) long a; int n; {return a>>n;}
unsigned long unsigned_shift(a,n) unsigned long a; int n; {return a>>n;}
unsigned long bits(a,b,n) unsigned long a,b;int n;{return ((a&b)^(a|b))+(a<<n);}
int less(a,b) long a;unsigned b;{return a<b;}
int unsigned_less(a,b) unsigned long a,b;{return a<b;}
int loop(n) int n; {int x;x=0;while(n>0){x+=n;n--;}return x;}
long *advance(p,n) long *p;int n;{return p+n;}
long distance(a,b) long *a,*b;{return a-b;}
int compound(p) int *p;{*p++ +=5;return *p;}
int post(p) int *p;{return (*p)++;}
int pre(p) int *p;{return ++*p;}
int narrow(a,b,c) char a;unsigned short b;int c;{return a+b+c;}
struct tiny {unsigned char value:3;};
int char_divide(p,n) unsigned char *p;int n;{return *p/=n;}
int field_divide(p,n) struct tiny *p;int n;{return p->value/=n;}
int field_less(p) struct tiny *p;{return p->value < -1;}
int choose(a,b,c) int a,b,c;{return a?b:c;}
int lazy(a) int a;{return a && (12/a);}
int dispatch(a) long a;{switch(a){case -1L:return 2;case 4294967297L:return 3;default:return 4;}}
int static_value(){static int value=3;value++;return value;}
int tiny_update(p) struct tiny *p;{return p->value+=2;}
struct fields {unsigned a:3;int b:5;unsigned c:9;};
int fields_less(p) struct fields *p;{return p->a < -1;}
int fields_update(p) struct fields *p;{p->a=7;p->b= -9;p->c=300;return p->a+p->b+p->c;}
'''
DRIVER='''#include <stdio.h>
#include <stdint.h>
extern long calc(long,unsigned),many(long,long,long,long,long,long,long,long),signed_divide(long,long),signed_remainder(long,long),signed_shift(long,int),distance(long *,long *);
extern unsigned long unsigned_divide(unsigned long,unsigned long),unsigned_remainder(unsigned long,unsigned long),unsigned_shift(unsigned long,int),bits(unsigned long,unsigned long,int);
extern int less(long,unsigned),unsigned_less(unsigned long,unsigned long),loop(int),compound(int *),post(int *),pre(int *),narrow(int,int,int),choose(int,int,int),lazy(int),dispatch(long),static_value(void);
extern long *advance(long *,int);
extern long indirect(void),cast_assignment(unsigned *),unary(long);
extern unsigned wrap32(unsigned,unsigned);
extern unsigned long huge_address(void);extern long global_value;
extern int literal(void),flow(int),no_default(int);
struct tiny {unsigned char value:3;}; extern int tiny_update(struct tiny *),field_divide(struct tiny *,int),field_less(struct tiny *),char_divide(unsigned char *,int);
struct fields {unsigned a:3;int b:5;unsigned c:9;};extern int fields_update(struct fields *),fields_less(struct fields *);
#define CHECK(x) do{if(!(x)){printf("FAIL integer line %d\\n",__LINE__);return 1;}}while(0)
int main(void){
 long a[8];unsigned cast_value=0;int p[2]={10,22};unsigned char guarded[2]={7,90};struct fields f={0};
 CHECK(huge_address()==(unsigned long)&global_value+0x100000000UL);
 CHECK(indirect()==4294967297L && literal()==211);CHECK(wrap32(0xffffffffU,1)==0);
 CHECK(cast_assignment(&cast_value)== -1L && cast_value==0xffffffffU);CHECK(unary(7)==6);
 CHECK(flow(10)==8 && no_default(0)==6 && no_default(3)==9);
 CHECK(calc(-2,4)==4294967303L);CHECK(calc(-1,0xffffffffU)==17179869179L);
 CHECK(many(1,2,3,4,5,6,7,8)==204);CHECK(signed_divide(-99,7)== -14);CHECK(signed_remainder(-99,7)== -1);
 CHECK(unsigned_divide(~0UL,3)==0x5555555555555555UL);CHECK(unsigned_remainder(~0UL,7)==1);
 CHECK(signed_shift(-1234567890123L,17)==(-1234567890123L>>17));CHECK(unsigned_shift(~0UL,61)==7);
 CHECK(bits(0x123456789abcdef0UL,0x9876543210fedcbaUL,3)==(((0x123456789abcdef0UL&0x9876543210fedcbaUL)^(0x123456789abcdef0UL|0x9876543210fedcbaUL))+(0x123456789abcdef0UL<<3)));
 CHECK(less(-1,0xffffffffU));CHECK(unsigned_less(0,~0UL));CHECK(!unsigned_less(~0UL,0));
 CHECK(loop(10)==55);CHECK(advance(a,3)==a+3);CHECK(distance(a+7,a+2)==5);
 CHECK(compound(p)==22 && p[0]==15 && p[1]==22);CHECK(post(p)==15 && p[0]==16);CHECK(pre(p)==17 && p[0]==17);
 CHECK(narrow(255,65535,-5)==65529);CHECK(choose(0,5,7)==7 && choose(1,5,7)==5);CHECK(lazy(0)==0 && lazy(3)==1);
 CHECK(dispatch(-1)==2 && dispatch(4294967297L)==3 && dispatch(1)==4);CHECK(static_value()==4 && static_value()==5);
 CHECK(tiny_update((struct tiny *)guarded)==1 && guarded[1]==90);
 guarded[0]=7;CHECK(field_divide((struct tiny *)guarded,-2)==5 && guarded[1]==90);CHECK(!field_less((struct tiny *)guarded));
 guarded[0]=250;CHECK(char_divide(guarded,256)==0 && guarded[0]==0);CHECK(fields_update(&f)==298 && f.a==7 && f.b== -9 && f.c==300);CHECK(!fields_less(&f));
 puts("PASS: PCC integer/pointer/control backend");return 0;
}
'''
PROGRAM='''unsigned long constant=0xffffffffffffffffUL;
int global_zero[4];
struct pair {int left;long right;};
struct pair object;
int main(){
 int array[4];int i;int sum;int lazy_value;long *pointer;
 array[0]=3;array[1]= -7;array[2]=11;array[3]=9;
 sum=0;for(i=0;i<4;i++)sum+=array[i];
 if(sum!=16)return 1;
 if(global_zero[0] || global_zero[3])return 2;
 if(constant/3UL!=6148914691236517205UL)return 3;
 object.left=7;object.right=4294967297L;
 if(object.left!=7 || object.right!=4294967297L)return 4;
 pointer = &object.right;if(*pointer!=4294967297L)return 5;
 lazy_value=0;if(0 && (lazy_value=1))return 6;if(lazy_value)return 7;
 if(!(1 || (lazy_value=2)))return 8;if(lazy_value)return 9;
 i=0;do{i++;}while(i<5);if(i!=5)return 10;
 goto finish;return 11;
 finish:return 0;
}
'''
(OUT/'integer.c').write_text(FUNCTIONS);(OUT/'int-driver.c').write_text(DRIVER);(OUT/'program.c').write_text(PROGRAM)
with tempfile.TemporaryDirectory(prefix='nx-pcc-integer-') as tmp:
    d=Path(tmp);binary=d/'ccom';flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    mip=[]
    for name in ('common','trees','match','allo'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','code','local2']
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['build/gen-pcc/cgram.c',*mip,'-o',str(binary)],cwd=ROOT,check=True)
    def compile(name):
        p=subprocess.run([str(binary),str(OUT/(name+'.c'))],capture_output=True,env=ENV,timeout=10)
        assert p.returncode==0 and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        asm=OUT/(name+'.s');asm.write_bytes(p.stdout)
        subprocess.run(['as','--64',str(asm),'-o',str(OUT/(name+'.o'))],check=True)
    compile('integer');compile('program')
    # Keep native assembler working sets bounded on the 16 MiB baseline while
    # exercising every case through native preprocessing/compilation/assembly.
    lines=FUNCTIONS.splitlines();types=[line for line in lines if line.startswith('struct ')]
    functions=[line for line in lines if '{' in line and not line.startswith('struct ')]
    modules=[]
    for start in range(0,len(functions),4):
        name='part'+str(start//4);modules.append(name)
        prefix='long global_value = 4294967297L;\nint zeros[8];\nlong *global_pointer = &global_value;\nchar *greeting = "native";\n' if start==0 else 'extern long global_value;\nextern int zeros[8];\nextern long *global_pointer;\nextern char *greeting;\n'
        (OUT/(name+'.c')).write_text(prefix+'\n'.join(types+functions[start:start+4])+'\n')
        compile(name)
    (OUT/'modules.json').write_text(json.dumps(modules))
    subprocess.run(['cc','-no-pie','-O2',str(OUT/'int-driver.c'),*[str(OUT/(name+'.o')) for name in modules],'-o',str(d/'modular')],check=True)
    subprocess.run([str(d/'modular')],check=True)
    subprocess.run(['cc','-no-pie','-O2',str(OUT/'int-driver.c'),str(OUT/'integer.o'),'-o',str(d/'generated')],check=True)
    subprocess.run([str(d/'generated')],check=True)
    # The same driver and sources through GCC form a separate behavior oracle.
    subprocess.run(['cc','-std=gnu89','-no-pie','-O2',str(OUT/'int-driver.c'),str(OUT/'integer.c'),'-o',str(d/'oracle')],check=True)
    subprocess.run([str(d/'oracle')],check=True)
    subprocess.run(['cc','-no-pie',str(OUT/'program.o'),'-o',str(d/'program')],check=True)
    subprocess.run([str(d/'program')],check=True)
    # Bootstrap only the external native caller; generated bodies remain PCC output.
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(OUT/'int-driver.c'),'-o',str(OUT/'int-driver.o')],cwd=ROOT,check=True)
print('PASS: PCC integer backend sanitizers, GNU caller/execution oracle, scalar widths, stack arguments, pointer/field stores, control flow and static data')

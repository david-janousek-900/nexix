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
long callback_call(f,x) long (*f)();long x;{return 7+(*f)(x,2L,3L,4L,5L,6L,7L,8L,9L);}
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
struct bundle {char tag;long values[3];char tail;};
int copy_bundle(p,q) struct bundle *p,*q;{*p = *q;return p->tag+p->tail;}
struct triplet {char a,b,c;};
int copy_triplet(p,q) struct triplet *p,*q;{*p = *q;return p->c;}
struct fields {unsigned a:3;int b:5;unsigned c:9;};
int fields_less(p) struct fields *p;{return p->a < -1;}
int fields_update(p) struct fields *p;{p->a=7;p->b= -9;p->c=300;return p->a+p->b+p->c;}
long long native_ll(a,b) long long a;unsigned b;{return a+b;}
unsigned long long native_ull(a,b) unsigned long long a,b;{return a/b;}
_Bool native_bool(a) long a;{return (_Bool)a;}
_Bool native_float_bool(a) double a;{return (_Bool)a;}
float native_float(float a,float b){return a+b;}
float native_float_call(float (*fn)(float,float)){return fn(1.25,2.5);}
int native_predicates(double x){return __builtin_isnan(x)+2*__builtin_isfinite(x)+4*__builtin_signbit(x);}
void native_store(int *p){*p=7;return;}
double fp_math(a,b) double a,b;{double x;x=(a+b)*(a-b);x/=b;return -x;}
float fp_float(a,b) double a,b;{static float scale=1.0;float x;x=a*scale;x+=b;return x++;}
unsigned long fp_to_unsigned(a) double a;{return (unsigned long)a;}
double unsigned_to_fp(a) unsigned long a;{return (double)a;}
long fp_to_signed(a) double a;{return (long)a;}
int fp_int_compound(p,b) int *p;double b;{return *p/=b;}
int fp_cmp(a,b) double a,b;{return (a==b)+2*(a!=b)+4*(a<b)+8*(a<=b)+16*(a>b)+32*(a>=b)+64*(!a);}
int fp_truth(a,b) double a,b;{if(a && b)return 1;if(a || b)return 2;return 3;}
double fp_callback(f,x) double (*f)();double x;{return (*f)(1,x,2,3.5);}
double fp_many(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q) double a,b,c,d,e,f,g,h,i,j;long k,l,m,n,o,p,q;{return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h+9*i+10*j+k+l+m+n+o+p+q;}

'''
DRIVER='''#include <stdio.h>
#include <stdarg.h>
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
long callback_target(long a,long b,long c,long d,long e,long f,long g,long h,long i){return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h+9*i;}
struct bundle {char tag;long values[3];char tail;};extern int copy_bundle(struct bundle *,struct bundle *);
struct triplet {char a,b,c;};extern int copy_triplet(struct triplet *,struct triplet *);
extern long long native_ll(long long,unsigned);
extern unsigned long long native_ull(unsigned long long,unsigned long long);
extern _Bool native_bool(long),native_float_bool(double);
extern float native_float(float,float),native_float_call(float (*)(float,float));
extern void native_store(int *);extern int native_predicates(double);
extern long callback_call(long (*)(),long);
extern double fp_math(double,double),unsigned_to_fp(unsigned long),fp_callback(double (*)(),double);
extern float fp_float(double,double);
extern unsigned long fp_to_unsigned(double);
extern long fp_to_signed(double);
extern int fp_cmp(double,double),fp_truth(double,double),fp_int_compound(int *,double);
extern double fp_many(double,double,double,double,double,double,double,double,double,double,long,long,long,long,long,long,long);
double fp_variadic(int first,...){va_list ap;double a,b;int i;va_start(ap,first);a=va_arg(ap,double);i=va_arg(ap,int);b=va_arg(ap,double);va_end(ap);return first+a+i+b;}

int main(void){
 long a[8];unsigned cast_value=0;int p[2]={10,22};unsigned char guarded[2]={7,90};struct fields f={0};
 {struct bundle from={3,{4294967297L,-2,99},7},to={0};
 CHECK(copy_bundle(&to,&from)==10 && to.values[0]==4294967297L && to.values[1]== -2 && to.values[2]==99);
 CHECK(copy_bundle(&to,&to)==10);}
 {struct {struct triplet v;char guard;} to={{0,0,0},99};struct triplet from={1,2,3};
 CHECK(copy_triplet(&to.v,&from)==3 && to.v.a==1 && to.v.b==2 && to.guard==99);}
 CHECK(native_ll(-1LL,0xffffffffU)==4294967294LL);
 CHECK(native_ull(~0ULL,3ULL)==0x5555555555555555ULL);
 CHECK(native_bool(256) && !native_bool(0) && native_bool(-1));
 CHECK(native_float_bool(0.5) && !native_float_bool(-0.0));
 CHECK(native_float(1.25f,2.5f)==3.75f && native_float_call(native_float)==3.75f);
 CHECK(native_predicates(-0.0)==6 && native_predicates(__builtin_nan(""))==1 && native_predicates(1.0/0.0)==0);
 native_store(p);CHECK(p[0]==7);p[0]=10;
 CHECK(callback_call(callback_target,1)==292);
 p[0]= -7;CHECK(fp_int_compound(p,2.5)== -2 && p[0]== -2);p[0]=10;
 CHECK(fp_math(7.5,2.5)== -20.0);CHECK(fp_float(1.25,2.5)==3.75f);
 CHECK(fp_to_unsigned(9223372036854775808.0)==0x8000000000000000UL);
 CHECK(fp_to_unsigned(18446744073709549568.0)==0xfffffffffffff800UL);
 CHECK(unsigned_to_fp(0xffffffffffffffffUL)==18446744073709551616.0);
 CHECK(fp_to_signed(-1234.75)== -1234);CHECK(fp_to_unsigned(1234.75)==1234UL);
 CHECK(fp_cmp(2.0,3.0)==14 && fp_cmp(3.0,2.0)==50 && fp_cmp(3.0,3.0)==41);
 CHECK(fp_cmp(0.0/0.0,2.0)==2);CHECK(fp_cmp(-0.0,0.0)==105);
 CHECK(fp_truth(0.0,0.0)==3 && fp_truth(1.0,0.0)==2 && fp_truth(1.0,2.0)==1);
 CHECK(fp_callback((double (*)())fp_variadic,1.25)==7.75);
 CHECK(fp_many(1,2,3,4,5,6,7,8,9,10,1,2,3,4,5,6,7)==413);
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
long sum9(a,b,c,d,e,f,g,h,i) long a,b,c,d,e,f,g,h,i;{return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h+9*i;}
long recurse(n) long n;{if(n<2)return 1;return n*recurse(n-1);}
long leaf(){return 4294967297L;}
double fp_sum(a,b,c,d,e,f,g,h,i) double a,b,c,d,e,f,g,h,i;{return a+b+c+d+e+f+g+h+i;}
double fp_leaf(){return 2.5;}
long nest(n) long n;{return sum9(n,leaf(),3L,4L,recurse(3L),6L,7L,8L,9L);}
int main(){
 int array[4];int i;int sum;int lazy_value;long *pointer;long (*fp)();
 fp = sum9;
 if((*fp)(1L,2L,3L,4L,5L,6L,7L,8L,9L)!=285L)return 20;
 if(recurse(10L)!=3628800L)return 21;
 if(fp_sum(1.0,fp_leaf(),3.0,4.0,5.0,6.0,7.0,8.0,9.0)!=45.5)return 24;
 if(fp_leaf()+fp_leaf()*2.0!=7.5)return 25;
 if(leaf()+recurse(4L)!=4294967321L)return 22;
 if(nest(1L)!=8589934880L)return 23;
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
# Exercise target formatting across its stack-buffer boundary and preserve the
# longest accepted symbol in text, IR names and a separately compiled caller.
long_symbol='shared_pass_'+('n'*243)
assert len(long_symbol)==255
FUNCTIONS+=f'long {long_symbol}(x) long x;{{return x+4294967297L;}}\n'
DRIVER=DRIVER.replace('int main(void){',f'extern long {long_symbol}(long);\nint main(void){{\n CHECK({long_symbol}(2L)==4294967299L);')
helper_lines=[line for line in PROGRAM.splitlines() if line.startswith(('long sum9(', 'long recurse(', 'long leaf(', 'long nest(', 'double fp_sum(', 'double fp_leaf('))]
PROGRAM='\n'.join(line for line in PROGRAM.splitlines() if line not in helper_lines)
PROGRAM=PROGRAM.replace('int main(){','long sum9(),recurse(),leaf(),nest();double fp_sum(),fp_leaf();\nint main(){')
(OUT/'integer.c').write_text(FUNCTIONS);(OUT/'int-driver.c').write_text(DRIVER);(OUT/'program.c').write_text(PROGRAM)
with tempfile.TemporaryDirectory(prefix='nx-pcc-integer-') as tmp:
    d=Path(tmp);binary=d/'ccom';flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    mip=[]
    for name in ('common','trees','match','allo','reader'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','declarations','real','abi','code','local2','output']
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['build/gen-pcc/cgram.c',*mip,'-o',str(binary)],cwd=ROOT,check=True)
    def compile(name):
        p=subprocess.run([str(binary),str(OUT/(name+'.c'))],capture_output=True,env=ENV,timeout=10)
        assert p.returncode==0 and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        # A distinct frontend process writes the shared pass; a fresh backend
        # process has no parser symbol/dimension state and must reproduce it.
        encoded=subprocess.run([str(binary),'--emit-pass',str(OUT/(name+'.c'))],capture_output=True,env=ENV,timeout=10)
        assert encoded.returncode==0 and encoded.stderr==p.stderr,encoded.stderr
        (OUT/(name+'.pass')).write_bytes(encoded.stdout)
        decoded=subprocess.run([str(binary),'--read-pass'],input=encoded.stdout,capture_output=True,env=ENV,timeout=10)
        assert decoded.returncode==0 and decoded.stdout==p.stdout and not decoded.stderr,decoded.stderr
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
    helpers=[]
    for start in range(0,len(helper_lines),3):
        name='helper'+str(start//3);helpers.append(name);modules.append(name)
        (OUT/(name+'.c')).write_text('long sum9(),recurse(),leaf(),nest();double fp_sum(),fp_leaf();\n'+'\n'.join(helper_lines[start:start+3])+'\n')
        compile(name)
    (OUT/'modules.json').write_text(json.dumps(modules))
    subprocess.run(['cc','-no-pie','-O2',str(OUT/'int-driver.c'),*[str(OUT/(name+'.o')) for name in modules],'-o',str(d/'modular')],check=True)
    subprocess.run([str(d/'modular')],check=True)
    subprocess.run(['cc','-no-pie','-O2',str(OUT/'int-driver.c'),str(OUT/'integer.o'),'-o',str(d/'generated')],check=True)
    subprocess.run([str(d/'generated')],check=True)
    # The same driver and sources through GCC form a separate behavior oracle.
    subprocess.run(['cc','-std=gnu89','-no-pie','-O2',str(OUT/'int-driver.c'),str(OUT/'integer.c'),'-o',str(d/'oracle')],check=True)
    subprocess.run([str(d/'oracle')],check=True)
    subprocess.run(['cc','-no-pie',str(OUT/'program.o'),*[str(OUT/(name+'.o')) for name in helpers],'-o',str(d/'program')],check=True)
    subprocess.run([str(d/'program')],check=True)
    # Bootstrap only the external native caller; generated bodies remain PCC output.
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(OUT/'int-driver.c'),'-o',str(OUT/'int-driver.o')],cwd=ROOT,check=True)
    # A failed frontend must not finish a valid stream. A separate backend
    # rejects it rather than accepting EOF as successful pass completion.
    failed=subprocess.run([str(binary),'--emit-pass'],input=b'int broken(',capture_output=True,env=ENV,timeout=10)
    assert failed.returncode!=0 and b'Sanitizer' not in failed.stderr
    rejected=subprocess.run([str(binary),'--read-pass'],input=failed.stdout,capture_output=True,env=ENV,timeout=10)
    assert rejected.returncode==1 and rejected.stderr==b'pcc: invalid or failed compiler pass\n',rejected
print('PASS: PCC integer backend sanitizers, GNU caller/execution oracle, scalar widths, stack arguments, pointer/field stores, control flow and static data')

/* GCC-built caller/callee half of the PCC ABI fixture. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
struct pair {int a,b;};
struct dpair {double x,y;};
struct mixed {long a;double b;};
struct fmix {float f,g;int c;};
struct odd {char c[3];};
struct big {long a,b,c;};
struct ext {long double v;};
struct sse_int {double d;long l;};
extern long double ld_global;
struct pair make_pair(int,int);int sum_pair(struct pair);
struct dpair scale_dpair(struct dpair,double);struct mixed swap_mixed(struct mixed);
struct fmix fmix_add(struct fmix,struct fmix);struct odd odd_next(struct odd);
struct big big_add(struct big,struct big);struct ext ext_twice(struct ext);
long spill_pairs(struct pair,struct pair,struct pair,struct pair,struct pair,struct pair,struct pair,struct pair);
double spill_dpairs(struct dpair,struct dpair,struct dpair,struct dpair,struct dpair);
long call_back_pair(struct pair (*)(int,int));long call_back_big(struct big (*)(struct big,struct big));
double call_back_sse_int(struct sse_int (*)(struct sse_int));int nested_struct(void);
long double ld_add(long double,long double),ld_ops(long double,long double);int ld_cmp(long double,long double);
long ld_to_long(long double);unsigned long ld_to_ulong(long double);long double long_to_ld(long),ulong_to_ld(unsigned long);
double ld_to_double(long double);float ld_to_float(long double);long double double_to_ld(double,float);
long double ld_compound(long double *,int),ld_global_step(void),ld_many(long double,int,double,long double,long,long double);
long double ld_call(long double (*)(long double,long double));int ld_truth(long double,long double),ld_predicates(long double);
long double ld_constant(void);int ld_bool(long double);
long va_sum(int,...);double va_mixed(int,...);long double va_ld(int,...);long va_struct(int,...);long va_copy_twice(int,...);
char *va_pointer(char *,...);struct pair va_return(int,...);long va_outer(int,...);
#define CHECK(x) do{if(!(x)){printf("FAIL abi line %d\n",__LINE__);return 1;}}while(0)
static struct pair gcc_pair(int a,int b){struct pair p={a*2,b*3};return p;}
static struct big gcc_big(struct big a,struct big b){struct big r={a.a*b.a,a.b*b.b,a.c*b.c};return r;}
static struct sse_int gcc_sse_int(struct sse_int s){s.d*=4;s.l*=5;return s;}
static long double gcc_ld(long double a,long double b){return a*b;}
union number {long l;double d;};
struct bits {unsigned a:3;int b:9;unsigned c:20;};
struct quad {float v[4];};
struct big make_big(long,long,long);struct big va_big(int,...);long double ld_array(void),ld_from_float(float);
union number union_echo(union number);int bits_sum(struct bits);struct quad quad_scale(struct quad);long kr_pair();
long double call_gcc_variadic(void);long pcc_to_pcc_variadic(void);
long double gcc_variadic(int n,...){va_list ap;long double s;struct pair p;struct big b;va_start(ap,n);s=va_arg(ap,long double);p=va_arg(ap,struct pair);b=va_arg(ap,struct big);s+=va_arg(ap,double);s+=va_arg(ap,long);va_end(ap);return s+n+p.a+p.b+b.a+b.b+b.c;}
int more(void){
 {struct big b=make_big(1,2,3);CHECK(b.a==1 && b.b==2 && b.c==3);b=va_big(9,10L,11L);CHECK(b.a==10 && b.b==11 && b.c==9);}
 CHECK(ld_array()==1.75L);CHECK(ld_from_float(0.5f)==0.5L);
 {union number u;u.l=41;u=union_echo(u);CHECK(u.l==42);}
 {struct bits b={5,-100,1000000};CHECK(bits_sum(b)==5-100+1000000);}
 {struct quad q={{1,2,3,4}};q=quad_scale(q);CHECK(q.v[0]==2 && q.v[3]==8);}
 {struct pair p={3,4};CHECK(kr_pair(p,10L)==34);}
 CHECK(call_gcc_variadic()==1.5L+2.5L+7+3+1+2+3+4+5);CHECK(pcc_to_pcc_variadic()==14);
 return 0;
}
int main(void){
 if(more())return 1;
 struct pair p=make_pair(3,-9);CHECK(p.a==3 && p.b==-9);CHECK(sum_pair(p)==-6);
 {struct dpair d={1.5,-2.25};d=scale_dpair(d,2.0);CHECK(d.x==3.0 && d.y==-4.5);}
 {struct mixed m={7,2.75};m=swap_mixed(m);CHECK(m.a==2 && m.b==7.0);}
 {struct fmix a={1.5f,2.5f,3},b={0.25f,0.5f,-10},r;r=fmix_add(a,b);CHECK(r.f==1.75f && r.g==3.0f && r.c==-7);}
 {struct odd o={{1,2,3}};o=odd_next(o);CHECK(o.c[0]==2 && o.c[1]==3 && o.c[2]==4);}
 {struct big a={1,2,3},b={4000000000L,5,-6},r;r=big_add(a,b);CHECK(r.a==4000000001L && r.b==7 && r.c==-3);}
 {struct ext e={1.25L};e=ext_twice(e);CHECK(e.v==2.5L);}
 {struct pair s[8];int i;for(i=0;i<8;i++){s[i].a=i+1;s[i].b=i*11;}
  CHECK(spill_pairs(s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7])==204+7700);}
 {struct dpair a={1,0},b={2,0},c={3,0},d={4,0},e={5,6};CHECK(spill_dpairs(a,b,c,d,e)==75.0);}
 CHECK(call_back_pair(gcc_pair)==6*10+12);CHECK(call_back_big(gcc_big)==10+40+90);
 CHECK(call_back_sse_int(gcc_sse_int)==16.0);CHECK(nested_struct()==13);
 CHECK(ld_add(1.25L,2.5L)==3.75L);CHECK(ld_ops(7.5L,2.5L)==-20.0L);
 CHECK(ld_cmp(2,3)==14 && ld_cmp(3,2)==50 && ld_cmp(3,3)==41 && ld_cmp(0.0L/0.0L,2)==2 && ld_cmp(-0.0L,0.0L)==105);
 CHECK(ld_to_long(-1234.75L)==-1234 && ld_to_ulong(18446744073709551615.0L)==18446744073709551615UL && ld_to_ulong(9223372036854775808.0L)==9223372036854775808UL && ld_to_ulong(12.9L)==12);
 CHECK(long_to_ld(-5)==-5.0L && ulong_to_ld(18446744073709551615UL)==18446744073709551615.0L && ulong_to_ld(3)==3.0L);
 CHECK(ld_to_double(0.1L)==(double)0.1L && ld_to_float(0.1L)==(float)0.1L && double_to_ld(0.5,0.25f)==0.75L);
 {long double x=1.5L;CHECK(ld_compound(&x,2)==8.0L && x==7.0L);}
 CHECK(ld_global_step()==4.75L && ld_global==4.75L);
 CHECK(ld_many(1.0L,2,3.0,4.0L,5,6.0L)==21.0L);CHECK(ld_call(gcc_ld)==3.125L+0.125L);
 CHECK(ld_truth(0,0)==3 && ld_truth(1,0)==2 && ld_truth(1,2)==1);
 CHECK(ld_predicates(-0.0L)==6 && ld_predicates(__builtin_nanl(""))==1 && ld_predicates(1.0L/0.0L)==0);
 CHECK(ld_constant()==0.1L);CHECK(ld_bool(0.5L)==1 && ld_bool(0.0L)==0);
 CHECK(va_sum(9,1L,2L,3L,4L,5L,6L,7L,8L,9L)==45);
 CHECK(va_mixed(6,1,0.5,2,0.25,3,0.125,4,1.0,5,2.0,6,4.0)==21+7.875);
 CHECK(va_mixed(10,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0,1,1.0)==20.0);
 CHECK(va_ld(3,1.5L,2.25L,-0.75L)==3.0L);
 {struct pair a={1,2};struct big b={3,4,5};struct dpair d={6.0,7.0};
  CHECK(va_struct(1,a,b,d)==28);CHECK(va_struct(3,a,b,d,a,b,d,a,b,d)==84);}
 CHECK(va_copy_twice(8,1,2,3,4,5,6,7,8)==396);
 {char x[2];CHECK(va_pointer(x,x,x+1)==x+1);}
 {struct pair a={4,5},r;r=va_return(10,a);CHECK(r.a==14 && r.b==5);}
 CHECK(va_outer(1,20L,3L)==44);
 puts("PASS: PCC aggregate/long-double/variadic ABI");return 0;
}

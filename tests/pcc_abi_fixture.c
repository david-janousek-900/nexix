/* PCC side of the System V aggregate, long double and variadic ABI fixture.
 * Types precede the functions; each function stays on one line so the test
 * can split bounded native translation units.
 */
struct pair {int a,b;};
struct dpair {double x,y;};
struct mixed {long a;double b;};
struct fmix {float f,g;int c;};
struct odd {char c[3];};
struct big {long a,b,c;};
struct ext {long double v;};
struct sse_int {double d;long l;};
union number {long l;double d;};
struct bits {unsigned a:3;int b:9;unsigned c:20;};
struct quad {float v[4];};
long double ld_global = 1.5L;
extern long double gcc_variadic(int,...);
struct pair make_pair(int a,int b){struct pair p;p.a=a;p.b=b;return p;}
int sum_pair(struct pair p){return p.a+p.b;}
struct dpair scale_dpair(struct dpair d,double k){d.x*=k;d.y*=k;return d;}
struct mixed swap_mixed(struct mixed m){struct mixed r;r.a=(long)m.b;r.b=(double)m.a;return r;}
struct fmix fmix_add(struct fmix a,struct fmix b){struct fmix r;r.f=a.f+b.f;r.g=a.g+b.g;r.c=a.c+b.c;return r;}
struct odd odd_next(struct odd o){o.c[0]++;o.c[1]++;o.c[2]++;return o;}
struct big big_add(struct big a,struct big b){struct big r;r.a=a.a+b.a;r.b=a.b+b.b;r.c=a.c+b.c;return r;}
struct ext ext_twice(struct ext e){e.v=e.v*2;return e;}
long spill_pairs(struct pair a,struct pair b,struct pair c,struct pair d,struct pair e,struct pair f,struct pair g,struct pair h){return a.a+b.a*2+c.a*3+d.a*4+e.a*5+f.a*6+g.a*7+h.a*8+h.b*100;}
double spill_dpairs(struct dpair a,struct dpair b,struct dpair c,struct dpair d,struct dpair e){return a.x+b.x+c.x+d.x+e.x+e.y*10;}
long call_back_pair(struct pair (*f)(int,int)){struct pair p;p=f(3,4);return p.a*10+p.b;}
long call_back_big(struct big (*f)(struct big,struct big)){struct big a,b,r;a.a=1;a.b=2;a.c=3;b.a=10;b.b=20;b.c=30;r=f(a,b);return r.a+r.b+r.c;}
double call_back_sse_int(struct sse_int (*f)(struct sse_int)){struct sse_int s;s.d=1.5;s.l=2;s=f(s);return s.d+s.l;}
int nested_struct(){return sum_pair(make_pair(5,6))+make_pair(1,2).b;}
long double ld_add(long double a,long double b){return a+b;}
long double ld_ops(long double a,long double b){long double x;x=(a-b)*(a+b);x/=b;return -x;}
int ld_cmp(long double a,long double b){return (a==b)+2*(a!=b)+4*(a<b)+8*(a<=b)+16*(a>b)+32*(a>=b)+64*(!a);}
long ld_to_long(long double a){return (long)a;}
unsigned long ld_to_ulong(long double a){return (unsigned long)a;}
long double long_to_ld(long a){return a;}
long double ulong_to_ld(unsigned long a){return a;}
double ld_to_double(long double a){return a;}
float ld_to_float(long double a){return a;}
long double double_to_ld(double a,float b){return a+(long double)b;}
long double ld_compound(long double *p,int n){*p+=n;*p*=2;(*p)++;return (*p)--;}
long double ld_global_step(){ld_global=ld_global*3+0.25L;return ld_global;}
long double ld_many(long double a,int b,double c,long double d,long e,long double f){return a+b+c+d+e+f;}
long double ld_call(long double (*f)(long double,long double)){return f(1.25L,2.5L)+f(0.5L,0.25L);}
int ld_truth(long double a,long double b){if(a && b)return 1;if(a || b)return 2;return 3;}
int ld_predicates(long double x){return __builtin_isnan(x)+2*__builtin_isfinite(x)+4*(__builtin_signbit(x)!=0);}
long double ld_constant(){return 0.1L;}
int ld_bool(long double x){return (_Bool)x;}
long va_sum(int n,...){va_list ap;long s;int i;va_start(ap,n);s=0;for(i=0;i<n;i++)s+=va_arg(ap,long);va_end(ap);return s;}
double va_mixed(int n,...){va_list ap;double s;int i;va_start(ap,n);s=0;for(i=0;i<n;i++){s+=va_arg(ap,int);s+=va_arg(ap,double);}va_end(ap);return s;}
long double va_ld(int n,...){va_list ap;long double s;int i;va_start(ap,n);s=0;for(i=0;i<n;i++)s+=va_arg(ap,long double);va_end(ap);return s;}
long va_struct(int n,...){va_list ap;long s;int i;struct pair p;struct big b;struct dpair d;va_start(ap,n);s=0;for(i=0;i<n;i++){p=va_arg(ap,struct pair);b=va_arg(ap,struct big);d=va_arg(ap,struct dpair);s+=p.a+p.b+b.a+b.b+b.c+(long)d.x+(long)d.y;}va_end(ap);return s;}
long va_copy_twice(int n,...){va_list ap,aq;long s;int i;va_start(ap,n);va_copy(aq,ap);s=0;for(i=0;i<n;i++)s+=va_arg(ap,int);for(i=0;i<n;i++)s+=10*va_arg(aq,int);va_end(aq);va_end(ap);return s;}
char *va_pointer(char *first,...){va_list ap;char *p;va_start(ap,first);p=va_arg(ap,char *);p=va_arg(ap,char *);va_end(ap);return p;}
struct pair va_return(int n,...){va_list ap;struct pair p;va_start(ap,n);p=va_arg(ap,struct pair);va_end(ap);p.a+=n;return p;}
long va_forward(va_list ap){return va_arg(ap,long)*2;}
long va_outer(int n,...){va_list ap;long a,b;va_start(ap,n);a=va_forward(ap);b=va_arg(ap,long);va_end(ap);return a+b+n;}
struct big make_big(long a,long b,long c){struct big r;r.a=a;r.b=b;r.c=c;return r;}
struct big va_big(int n,...){va_list ap;struct big r;va_start(ap,n);r.a=va_arg(ap,long);r.b=va_arg(ap,long);r.c=n;va_end(ap);return r;}
long double ld_array(){static long double t=2.5L;long double a[3];int i;long double s;a[0]=t;a[1]=-1;a[2]=0.25L;s=0;for(i=0;i<3;i++)s+=a[i];return s;}
long double ld_from_float(float f){return f;}
union number union_echo(union number u){u.l+=1;return u;}
int bits_sum(struct bits b){return b.a+b.b+b.c;}
struct quad quad_scale(struct quad q){int i;for(i=0;i<4;i++)q.v[i]*=2;return q;}
long kr_pair(p,k) struct pair p; long k; {return p.a*k+p.b;}
long double call_gcc_variadic(){struct pair p;struct big b;p.a=1;p.b=2;b.a=3;b.b=4;b.c=5;return gcc_variadic(3,1.5L,p,b,2.5,7L);}
long pcc_to_pcc_variadic(){struct pair p;struct big b;struct dpair d;p.a=1;p.b=1;b.a=1;b.b=1;b.c=1;d.x=1;d.y=1;return va_struct(2,p,b,d,p,b,d);}

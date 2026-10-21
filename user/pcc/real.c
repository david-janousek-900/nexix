/* Decimal/hexadecimal literals rounded directly to binary24/53/64. Integer
 * ratios avoid losing x87 precision through the libc binary64 interface. */
#include "mfile1"
#define WORDS 2048
struct big {unsigned n;uint32_t v[WORDS];};
static void trim(struct big *a) {while(a->n && !a->v[a->n-1])a->n--;}
static void multiply(struct big *a,unsigned by,unsigned add) {
    uint64_t carry=add;unsigned i;
    for(i=0;i<a->n;i++) {carry+=(uint64_t)a->v[i]*by;a->v[i]=(uint32_t)carry;carry>>=32;}
    if(carry) {if(a->n==WORDS)cerror("floating literal too large");a->v[a->n++]=(uint32_t)carry;}
}
static unsigned bitlength(const struct big *a) {
    if(!a->n)return 0;uint32_t v=a->v[a->n-1];unsigned n=32*(a->n-1);while(v) {n++;v>>=1;}return n;
}
static void shift(struct big *a,unsigned bits) {
    if(!a->n || !bits)return;
    unsigned words=bits/32,rest=bits%32,i;uint64_t carry=0;
    if(a->n+words+1>WORDS)cerror("floating literal exponent too large");
    memmove(a->v+words,a->v,a->n*sizeof(*a->v));memset(a->v,0,words*sizeof(*a->v));a->n+=words;
    for(i=words;i<a->n;i++) {uint64_t v=((uint64_t)a->v[i]<<rest)|carry;a->v[i]=(uint32_t)v;carry=v>>32;}
    if(carry)a->v[a->n++]=(uint32_t)carry;
}
static int compare(const struct big *a,const struct big *b) {
    if(a->n!=b->n)return a->n>b->n?1:-1;unsigned i=a->n;
    while(i--)if(a->v[i]!=b->v[i])return a->v[i]>b->v[i]?1:-1;return 0;
}
static void subtract(struct big *a,const struct big *b) {
    uint64_t borrow=0;unsigned i;
    for(i=0;i<a->n;i++) {uint64_t v=(i<b->n?b->v[i]:0)+borrow,old=a->v[i];a->v[i]=(uint32_t)(old-v);borrow=old<v;}
    trim(a);
}
static int digit(unsigned c) {return c>='0' && c<='9'?(int)c-'0':c>='a' && c<='f'?(int)c-'a'+10:c>='A' && c<='F'?(int)c-'A'+10:-1;}
long double pcc_real(const char *text,TWORD *type) {
    struct big *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b)),*work=calloc(1,sizeof(*work));
    if(!a || !b || !work)cerror("no memory for floating literal");
    const char *p=text;int hex=p[0]=='0' && (p[1]=='x' || p[1]=='X'),fraction=0,seen=0,scale=0;
    unsigned base=hex?16:10;long double value=0;*type=DOUBLE;
    if(hex)p+=2;
    for(;;p++) {
        int d=digit((unsigned char)*p);
        if(*p=='.' && !fraction) {fraction=1;continue;}
        if(d<0 || (unsigned)d>=base)break;
        multiply(a,base,(unsigned)d);seen=1;if(fraction)scale-=hex?4:1;
    }
    if(!seen)goto invalid;
    if(*p==(hex?'p':'e') || *p==(hex?'P':'E')) {
        int negative=0,power=0;p++;if(*p=='+' || *p=='-')negative=*p++=='-';
        if(*p<'0' || *p>'9')goto invalid;
        while(*p>='0' && *p<='9') {if(power<100000)power=power*10+*p-'0';p++;}
        if(power>(hex?40000:10000)) {if(a->n && !negative)uerror("floating literal overflow");goto done;}
        scale+=negative?-power:power;
    } else if(hex)goto invalid;
    if((*p=='f' || *p=='F') && !p[1]) {*type=FLOAT;p++;}
    else if((*p=='l' || *p=='L') && !p[1]) {*type=LDOUBLE;p++;}
    if(*p)goto invalid;
    if(!a->n)goto done;
    b->n=1;b->v[0]=1;
    if(!hex) {int i;if(scale>=0)for(i=0;i<scale;i++)multiply(a,5,0);else for(i=0;i>scale;i--)multiply(b,5,0);}
    int exponent=(int)bitlength(a)-(int)bitlength(b),precision=*type==FLOAT?24:*type==DOUBLE?53:64;
    int minimum=*type==FLOAT?-126:*type==DOUBLE?-1022:-16382,maximum=*type==FLOAT?127:*type==DOUBLE?1023:16383;
    if(exponent>=0) {*work=*b;shift(work,(unsigned)exponent);if(compare(a,work)<0)exponent--;}
    else {*work=*a;shift(work,(unsigned)-exponent);if(compare(work,b)<0)exponent--;}
    exponent+=scale;
    if(exponent>maximum) {uerror("floating literal overflow");goto done;}
    if(exponent<minimum-precision)goto done;
    int unit=(exponent<minimum?minimum:exponent)-(precision-1),amount=scale-unit,i;
    if(amount>=0)shift(a,(unsigned)amount);else shift(b,(unsigned)-amount);
    uint64_t q=0;
    for(i=63;i>=0;i--) {*work=*b;shift(work,(unsigned)i);if(compare(a,work)>=0) {subtract(a,work);q|=UINT64_C(1)<<i;}}
    shift(a,1);int halfway=compare(a,b);
    if(halfway>0 || (!halfway && (q&1))) {if(q==UINT64_MAX) {q=UINT64_C(1)<<63;unit++;}else q++;}
    value=(long double)q;
    while(unit>0) {value*=2;unit--;}
    while(unit<0) {value/=2;unit++;}
    if(*type==FLOAT)value=(long double)(float)value;
    else if(*type==DOUBLE)value=(long double)(double)value;
    {unsigned short exponent_bits;memcpy(&exponent_bits,(char *)&value+8,2);if((exponent_bits&0x7fff)==0x7fff)uerror("floating literal overflow");}
    goto done;
invalid:uerror("invalid floating constant");
done:free(a);free(b);free(work);return value;
}

static int punctuation(void) {
    int c;do {c=getchar();if(c=='\n')pcc_newline();}while(c==' ' || c=='\t' || c=='\r' || c=='\n');return c;
}
int pcc_special_real(int nan) {
    if(punctuation()!='(' || (nan && (punctuation()!='"' || getchar()!='"')) || punctuation()!=')')uerror("invalid native special floating literal");
    unsigned char bytes[16]={0};uint64_t lo=UINT64_C(0x8000000000000000)|(nan?UINT64_C(0x4000000000000000):0);unsigned short hi=0x7fff;
    memcpy(bytes,&lo,8);memcpy(bytes+8,&hi,2);memcpy(&dcon,bytes,16);fcon_type=DOUBLE;return FCON;
}
NODE *pcc_predicate(unsigned op,NODE *argument) {
    TWORD t=argument->type;
    if(t!=FLOAT && t!=DOUBLE && t!=LDOUBLE)uerror("floating predicate requires floating type");
    if(argument->op==FCON) {
        uint64_t lo;unsigned short hi;memcpy(&lo,&argument->dval,8);memcpy(&hi,(char *)&argument->dval+8,2);
        int result=op==FP_NAN?(hi&0x7fff)==0x7fff && lo!=UINT64_C(0x8000000000000000):op==FP_FINITE?(hi&0x7fff)!=0x7fff:hi>>15;
        argument->op=FREE;return bcon(result);
    }
    return block((int)op,argument,NIL,INT,0,INT);
}

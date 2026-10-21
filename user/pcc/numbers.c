/* Native integer constants and usual arithmetic conversions for LP64 PCC. */
#include "mfile1"
#include <ctype.h>
TWORD pcc_promote(TWORD t) {
    if(t==BOOL || t==CHAR || t==SHORT || t==UCHAR || t==USHORT || t==ENUMTY || t==MOETY)return INT;
    return t;
}
TWORD pcc_value_type(NODE *n) {
    if(n->op==FLD && n->type==UNSIGNED && UPKFSZ(n->rval)<SZINT)return INT;
    return pcc_promote(n->type);
}
TWORD pcc_arithmetic(TWORD a,TWORD b) {
    a=pcc_promote(a);b=pcc_promote(b);
    if(a==LDOUBLE || b==LDOUBLE)return LDOUBLE;
    if(a==DOUBLE || b==DOUBLE)return DOUBLE;
    if(a==FLOAT || b==FLOAT)return FLOAT;
    if(a==ULLONG || b==ULLONG)return ULLONG;
    if(a==LLONG || b==LLONG)return a==ULONG || b==ULONG?ULLONG:LLONG;
    if(a==ULONG || b==ULONG)return ULONG;
    if(a==LONG || b==LONG)return LONG; /* unsigned int fits signed native long */
    if(a==UNSIGNED || b==UNSIGNED)return UNSIGNED;
    return INT;
}
uint64_t pcc_integer_bits(uint64_t value,TWORD t) {
    if(t==BOOL)return value!=0;
    struct mip_layout l;if(mip_scalar_layout(t,&l) || l.size==8)return value;
    unsigned width=(unsigned)l.size*8;uint64_t mask=(UINT64_C(1)<<width)-1;
    value&=mask;if(!ISUNSIGNED(t) && (value&(UINT64_C(1)<<(width-1))))value|=~mask;
    return value;
}
int pcc_conval(NODE *p,int op,NODE *q) {
    if(nerrors)return 0;
    int unary=op==UNARY MINUS || op==COMPL || op==NOT;
    if(p->rval!=NONAME || (!unary && q->rval!=NONAME)) {
        if(unary || (p->rval!=NONAME && q->rval!=NONAME) ||
           (op!=PLUS && (op!=MINUS || q->rval!=NONAME)))return 0;
    }
    if((op==ANDAND || op==OROR) && (p->op!=ICON || q->op!=ICON))return 0;
    TWORD type=unary || op==LS || op==RS?pcc_promote(p->type):pcc_arithmetic(p->type,q->type);
    if((p->type&TMASK) || (!unary && (q->type&TMASK)))type=ULONG;
    uint64_t a=pcc_integer_bits((uint64_t)p->lval,p->type),b=pcc_integer_bits((uint64_t)q->lval,q->type),result=0;
    a=pcc_integer_bits(a,type);b=pcc_integer_bits(b,type);int uns=ISUNSIGNED(type),logical=0;
    switch(op) {
    case PLUS:result=a+b;break;case MINUS:result=a-b;break;case MUL:result=a*b;break;
    case DIV:case MOD:
        if(!b) {uerror("division by zero");return 0;}
        if(uns)result=op==DIV?a/b:a%b;
        else {
            int64_t x=(int64_t)a,y=(int64_t)b;
            if(x==INT64_MIN && y==-1) {uerror("integer division overflow");return 0;}
            result=(uint64_t)(op==DIV?x/y:x%y);
        }
        break;
    case LS:case RS: {
        unsigned width=type==LONG || type==ULONG || type==LLONG || type==ULLONG || ISPTR(type)?64:32;
        if((uint64_t)q->lval>=width) {uerror("invalid constant shift");return 0;}
        result=op==LS?a<<(unsigned)q->lval:uns?a>>(unsigned)q->lval:(uint64_t)((int64_t)a>>(unsigned)q->lval);break;
    }
    case ANDAND:result=!!a && !!b;logical=1;break;case OROR:result=!!a || !!b;logical=1;break;
    case AND:result=a&b;break;case OR:result=a|b;break;case ER:result=a^b;break;
    case UNARY MINUS:result=0-a;break;case COMPL:result=~a;break;case NOT:result=!a;logical=1;break;
    case LT:case ULT:result=uns || op==ULT?a<b:(int64_t)a<(int64_t)b;logical=1;break;
    case LE:case ULE:result=uns || op==ULE?a<=b:(int64_t)a<=(int64_t)b;logical=1;break;
    case GT:case UGT:result=uns || op==UGT?a>b:(int64_t)a>(int64_t)b;logical=1;break;
    case GE:case UGE:result=uns || op==UGE?a>=b:(int64_t)a>=(int64_t)b;logical=1;break;
    case EQ:result=a==b;logical=1;break;case NE:result=a!=b;logical=1;break;
    default:return 0;
    }
    if(!logical && (op==PLUS || op==MINUS) && (p->type&TMASK))type=p->type;
    else if(!logical && op==PLUS && (q->type&TMASK))type=q->type;
    else if(p->rval!=NONAME)type=p->type;
    else if(!unary && q->rval!=NONAME) {p->rval=q->rval;type=q->type;}
    else if(logical)type=INT;
    p->type=type;p->csiz=(int)BTYPE(type);p->lval=(int64_t)pcc_integer_bits(result,type);return 1;
}
int pcc_number(int first) {
    char text[4096];size_t count=0;int c=first,hex=0;
    for(;;) {
        if(count==sizeof(text)-1) {uerror("numeric token too long");break;}
        text[count++]=(char)c;
        if(count==2 && text[0]=='0' && (c=='x' || c=='X'))hex=1;
        c=getchar();
        if(isalnum(c) || c=='.')continue;
        if((c=='+' || c=='-') && count && ((!hex && (text[count-1]=='e' || text[count-1]=='E')) || (hex && (text[count-1]=='p' || text[count-1]=='P'))))continue;
        break;
    }
    if(c!=EOF)ungetc(c,stdin);text[count]=0;
    int floating=0;size_t i;for(i=0;i<count;i++)if(text[i]=='.' || (!hex && (text[i]=='e' || text[i]=='E')) || (hex && (text[i]=='p' || text[i]=='P')))floating=1;
    if(floating) {
        dcon=pcc_real(text,&fcon_type);return FCON;
    }
    unsigned base=10;size_t start=0;
    if(hex) {base=16;start=2;}else if(text[0]=='0')base=8;
    uint64_t value=0;size_t digits=0;
    for(i=start;i<count;i++) {
        unsigned d=(unsigned char)text[i];
        d=d>='0' && d<='9'?d-'0':d>='a' && d<='f'?d-'a'+10:d>='A' && d<='F'?d-'A'+10:99;
        if(d>=base)break;
        if(value>(UINT64_MAX-d)/base) {uerror("integer constant too large");value=0;}
        value=value*base+d;digits++;
    }
    unsigned u=0,l=0;
    if(text[i]=='u' || text[i]=='U') {u=1;i++;}
    if(text[i]=='l' || text[i]=='L') {char letter=text[i++];l=1;if(text[i]==letter) {l=2;i++;}}
    if(!u && (text[i]=='u' || text[i]=='U')) {u=1;i++;}
    if(text[i])uerror("invalid integer constant suffix");
    if(!digits)uerror("integer constant has no digits");
    TWORD type;
    if(l==2)type=u || value>INT64_MAX?ULLONG:LLONG;
    else if(u)type=l || value>UINT32_MAX?ULONG:UNSIGNED;
    else if(l)type=value>INT64_MAX?ULONG:LONG;
    else if(value<=INT32_MAX)type=INT;
    else if(base!=10 && value<=UINT32_MAX)type=UNSIGNED;
    else type=value<=INT64_MAX?LONG:ULONG;
    lastcon=(int64_t)value;yylval.intval=(int)type;return ICON;
}

int pcc_newline(void) {if(lineno==INT32_MAX){uerror("source line number overflow");exit(1);}return ++lineno;}

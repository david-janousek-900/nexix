/* Native replacement for V7 cpy.y/yylex.c: LP64 values and lazy evaluation.
 * The macro scanner remains the supplied V7 scanner. Arithmetic wraps in
 * unsigned storage, avoiding host signed overflow in preprocessor expressions.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
struct symtab { char *name, *value; };
extern struct symtab *lookup(char *, int);
extern char *inp, *outp, *newp;
extern int flslvl, ifno, lineno[];
extern char *skipbl(char *);
extern void pperror(const char *, ...);
enum { NUMBER=256, END, DEFINED, EQ, NE, LE, GE, LS, RS, AND, OR };
typedef struct { unsigned long n; int u; } Value;
int cpp_expression;
static Value literal;
static int token, failed, held, depth, expression_line;
static Value value(unsigned long n, int u) { Value v; v.n=n; v.u=u; return v; }
static void error(const char *s) {
    int saved=lineno[ifno];lineno[ifno]=expression_line;
    if (!failed) pperror("%s in preprocessor if",s);
    lineno[ifno]=saved;failed=1;
}
static int digit(int c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
static void lex(void) {
    static const char *ops[]={"==","!=","<=",">=","<<",">>","&&","||"};
    static const int codes[]={EQ,NE,LE,GE,LS,RS,AND,OR};
    char save,*s,*end; int i,base,d,u,l,seen;
    unsigned long n;
again:
    newp=skipbl(newp);
    if(*inp=='\n') { token=END; return; }
    save=*newp; *newp=0; s=inp; end=newp; token=NUMBER; literal=value(0,0);
    for(i=0;i<8;i++) if(!strcmp(s,ops[i])) {token=codes[i]; goto done;}
    if(s[0] && !s[1] && strchr("+-*/%<>&^|?:!~(),",s[0])) {token=s[0]; goto done;}
    if(s[0]>='0'&&s[0]<='9') {
        base=10; n=0; u=l=seen=0;
        if(s[0]=='0') {base=8; if(s[1]=='x'||s[1]=='X') {base=16;s+=2;}}
        while((d=digit(*s))>=0 && d<base) {
            seen=1; if(n>(ULONG_MAX-d)/(unsigned)base) error("integer constant overflow");
            n=n*base+d; s++;
        }
        while(*s) {
            if((*s=='u'||*s=='U')&&!u) u=1;
            else if((*s=='l'||*s=='L')&&l<2) l++;
            else error("invalid integer constant");
            s++;
        }
        if(!seen) error("invalid integer constant");
        literal=value(n,u||n>LONG_MAX); goto done;
    }
    if((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||*s=='_') {
        if(!strcmp(s,"defined")&&!held) { token=DEFINED; held=1; flslvl++; }
        else if(held) {literal=value(lookup(s,-1)->value!=0,0); held=0;flslvl--;}
        goto done;
    }
    if(*s=='\'') {
        s++;n=0;seen=0;
        while(s<end && *s!='\'') {
            d=(unsigned char)*s++;
            if(d=='\\' && s<end) {
                d=*s++;
                switch(d) {
                case 'a':d=7;break;case 'b':d=8;break;case 't':d=9;break;
                case 'n':d=10;break;case 'v':d=11;break;case 'f':d=12;break;case 'r':d=13;break;
                default:
                    if(d>='0'&&d<='7') {d-='0';for(i=1;i<3&&*s>='0'&&*s<='7';i++) d=d*8+*s++-'0';}
                    break;
                }
            }
            if(++seen>sizeof(long)) error("character constant too long");
            n=(n<<8)|(unsigned char)d;
        }
        if(!seen||*s!='\''||s+1!=end) error("invalid character constant");
        literal=value(n,0);goto done;
    }
    if(!strcmp(s,"\\\n")) { *newp=save;outp=inp=newp; goto again; }
    error("illegal token");
done:
    *newp=save;outp=inp=newp;
}
static int precedence(int t) {
    switch(t) {
    case ',':return 1;case '?':return 2;case OR:return 3;case AND:return 4;
    case '|':return 5;case '^':return 6;case '&':return 7;
    case EQ:case NE:return 8;case '<':case '>':case LE:case GE:return 9;
    case LS:case RS:return 10;case '+':case '-':return 11;
    case '*':case '/':case '%':return 12;default:return 0;
    }
}
static Value expression(int,int);
static Value unary(int active) {
    Value a; int op=token,paren;
    if(++depth>128) { error("expression nesting limit");depth--;return value(0,0); }
    if(op=='+'||op=='-'||op=='!'||op=='~') {
        lex();a=unary(active);
        if(op=='-') a.n=0-a.n;
        if(op=='~') a.n=~a.n;
        if(op=='!') a=value(!a.n,0);
    } else if(op=='(') {
        lex();a=expression(1,active);
        if(token!=')') error("missing closing parenthesis");else lex();
    } else if(op==DEFINED) {
        lex();paren=token=='(';if(paren) lex();
        a=literal;
        if(token!=NUMBER || held) error("expected identifier after defined");else lex();
        if(paren) {if(token!=')') error("missing defined parenthesis");else lex();}
    } else if(op==NUMBER) {a=literal;lex();}
    else {error("expected expression");a=value(0,0);if(token!=END) lex();}
    depth--;return a;
}
static Value binary(int op,Value a,Value b,int active) {
    int u=a.u||b.u; unsigned long x=a.n,y=b.n;
    long sx=(long)x,sy=(long)y;
    switch(op) {
    case '+':return value(x+y,u);case '-':return value(x-y,u);case '*':return value(x*y,u);
    case '/':case '%':
        if(!active)return value(0,u);
        if(!y || (!u&&sx==LONG_MIN&&sy==-1)) {error("invalid division");return value(0,u);}
        return value(op=='/'?(u?x/y:(unsigned long)(sx/sy)):(u?x%y:(unsigned long)(sx%sy)),u);
    case LS:case RS:
        if(y>=64) {if(active)error("invalid shift count");return value(0,a.u);}
        return value(op==LS?x<<y:(a.u?x>>y:(unsigned long)(sx>>y)),a.u);
    case '<':return value(u?x<y:sx<sy,0);case '>':return value(u?x>y:sx>sy,0);
    case LE:return value(u?x<=y:sx<=sy,0);case GE:return value(u?x>=y:sx>=sy,0);
    case EQ:return value(x==y,0);case NE:return value(x!=y,0);
    case '&':return value(x&y,u);case '^':return value(x^y,u);case '|':return value(x|y,u);
    case AND:return value(x&&y,0);case OR:return value(x||y,0);case ',':return b;
    default:return value(0,0);
    }
}
static Value expression(int min,int active) {
    Value a,b,c;int op,p;
    if(++depth>128) {error("expression nesting limit");depth--;return value(0,0);}
    a=unary(active);
    while(!failed && (p=precedence(token))>=min) {
        op=token;lex();
        if(op=='?') {
            b=expression(1,active&&a.n!=0);
            if(token!=':') {error("missing conditional colon");depth--;return a;}lex();
            c=expression(p,active&&a.n==0);
            a=value(a.n?b.n:c.n,b.u||c.u);
        } else {
            b=expression(p+1,active && !(op==AND&&!a.n) && !(op==OR&&a.n));
            a=binary(op,a,b,active);
        }
    }
    depth--;return a;
}
int yyparse(void) {
    Value a;int before=flslvl;
    cpp_expression=1;expression_line=lineno[ifno];failed=held=depth=0;lex();a=expression(1,1);
    if(token!=END) error("trailing tokens");
    while(token!=END) lex();
    flslvl=before;cpp_expression=0;
    return !failed && a.n!=0;
}

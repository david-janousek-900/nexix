/* Native replacement for the supplied PDP-11 assembler's first-pass scanner.
 * AT&T operands and explicit ELF sections replace PDP addressing/directives.
 */
#include "as.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
struct parser {struct as_program *p;const char *path;char *s;unsigned line,depth;size_t section,previous;unsigned local[10];};
struct as_expr as_absolute(uint64_t n) {struct as_expr e={n,AS_NONE,AS_NONE};return e;}
static char *copy(const char *s) {size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p;}
static void error(struct parser *c,const char *why) {
    if(!c->p->errors)fprintf(stderr,"as: %s:%u: %s\n",c->path,c->line,why);
    c->p->errors++;
}
static void blank(struct parser *c) {while(*c->s==' '||*c->s=='\t'||*c->s=='\r')c->s++;}
static int idchar(int c) {return isalnum((unsigned char)c)||c=='_'||c=='.'||c=='$';}
static int identifier(struct parser *c,char *name,size_t size) {
    blank(c);size_t n=0;
    while(idchar(*c->s)) {if(n+1>=size){error(c,"identifier too long");return -1;}name[n++]=*c->s++;}
    name[n]=0;if(!n){error(c,"expected identifier");return -1;}return 0;
}
size_t as_symbol(struct as_program *p,const char *name) {
    for(size_t i=0;i<p->symbol_count;i++)if(!strcmp(p->symbols[i].name,name))return i;
    if(p->symbol_count>=16384)return AS_NONE;
    if(p->symbol_count==p->symbol_capacity) {
        size_t n=p->symbol_capacity?p->symbol_capacity*2:64;
        void *next=realloc(p->symbols,n*sizeof(*p->symbols));if(!next)return AS_NONE;
        p->symbols=next;p->symbol_capacity=n;
    }
    size_t i=p->symbol_count++;memset(p->symbols+i,0,sizeof(*p->symbols));
    p->symbols[i].name=copy(name);p->symbols[i].section=AS_NONE;p->symbols[i].expression=as_absolute(0);
    return p->symbols[i].name?i:AS_NONE;
}
static size_t section(struct parser *c,const char *name,uint64_t flags,unsigned type) {
    for(size_t i=0;i<c->p->section_count;i++)if(!strcmp(c->p->sections[i].name,name))return i;
    if(c->p->section_count>=256){error(c,"too many sections");return AS_NONE;}
    size_t i=c->p->section_count;
    void *next=realloc(c->p->sections,(i+1)*sizeof(*c->p->sections));if(!next){error(c,"out of memory");return AS_NONE;}
    c->p->sections=next;struct as_section s={copy(name),flags,1,type};
    if(!s.name){error(c,"out of memory");return AS_NONE;}
    c->p->sections[i]=s;c->p->section_count++;return i;
}
static struct as_record *record(struct parser *c,enum as_kind kind) {
    struct as_program *p=c->p;
    if(p->count>=100000){error(c,"too many statements");return NULL;}
    if(p->count==p->capacity) {
        size_t n=p->capacity?p->capacity*2:64;
        void *next=realloc(p->records,n*sizeof(*p->records));if(!next){error(c,"out of memory");return NULL;}
        p->records=next;p->capacity=n;
    }
    struct as_record *r=p->records+p->count++;memset(r,0,sizeof(*r));
    r->kind=kind;r->line=c->line;r->section=c->section;r->symbol=AS_NONE;r->expression=as_absolute(0);return r;
}
static int digit(int c) {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static struct as_expr expression(struct parser *,int);
static struct as_expr primary(struct parser *c);
static struct as_expr primary_impl(struct parser *c) {
    blank(c);struct as_expr e=as_absolute(0);char name[256];
    if(*c->s=='+'||*c->s=='-'||*c->s=='~') {
        int op=*c->s++;e=primary(c);
        if(op=='-'){size_t n=e.plus;e.plus=e.minus;e.minus=n;e.value=0-e.value;}
        if(op=='~'){if(e.plus!=AS_NONE||e.minus!=AS_NONE)error(c,"nonconstant complement");e.value=~e.value;}
        return e;
    }
    if(*c->s=='(') {c->s++;e=expression(c,1);blank(c);if(*c->s!=')')error(c,"missing expression parenthesis");else c->s++;return e;}
    if(isdigit((unsigned char)*c->s)) {
        if(c->s[0]>='0'&&c->s[0]<='9'&&(c->s[1]=='f'||c->s[1]=='b')&&!idchar(c->s[2])) {
            unsigned n=c->s[0]-'0',v=c->local[n];if(c->s[1]=='f')v++;else if(!v)error(c,"missing backward local label");
            snprintf(name,sizeof(name),".N%u_%u",n,v);c->s+=2;e.plus=as_symbol(c->p,name);if(e.plus==AS_NONE)error(c,"out of symbol space");return e;
        }
        unsigned base=10;if(c->s[0]=='0') {base=8;if(c->s[1]=='x'||c->s[1]=='X'){base=16;c->s+=2;}}
        int seen=0,d;
        while((d=digit(*c->s))>=0&&(unsigned)d<base) {seen=1;if(e.value>(UINT64_MAX-d)/base)error(c,"integer overflow");e.value=e.value*base+d;c->s++;}
        if(!seen||idchar(*c->s))error(c,"invalid integer");
        return e;
    }
    if(identifier(c,name,sizeof(name)))return e;
    e.plus=!strcmp(name,".")?AS_DOT:as_symbol(c->p,name);
    if(e.plus==AS_NONE)error(c,"out of symbol space");
    else if(e.plus!=AS_DOT) {
        struct as_symbol *s=c->p->symbols+e.plus;
        if(s->defined==2&&s->expression.plus==AS_NONE&&s->expression.minus==AS_NONE)e=s->expression;
    }
    return e;
}
static struct as_expr primary(struct parser *c) {
    if(++c->depth>128){error(c,"expression nesting limit");c->depth--;return as_absolute(0);}
    struct as_expr e=primary_impl(c);c->depth--;return e;
}
static int precedence(const char *s,int *len) {
    *len=1;if((*s=='<'&&s[1]=='<')||(*s=='>'&&s[1]=='>')){*len=2;return 4;}
    switch(*s){case '|':return 1;case '^':return 2;case '&':return 3;case '+':case '-':return 5;case '*':case '/':case '%':return 6;default:return 0;}
}
static int add_symbol(struct as_expr *e,size_t s,int sign) {
    if(s==AS_NONE)return 0;
    size_t *same=sign>0?&e->plus:&e->minus,*opposite=sign>0?&e->minus:&e->plus;
    if(*opposite==s){*opposite=AS_NONE;return 0;}if(*same!=AS_NONE)return -1;*same=s;return 0;
}
static struct as_expr expression(struct parser *c,int min) {
    struct as_expr a=primary(c);int n,p;
    while(!c->p->errors) {
        blank(c);p=precedence(c->s,&n);if(p<min)break;
        int op=*c->s;c->s+=n;struct as_expr b=expression(c,p+1);
        if(op=='+'||op=='-') {
            if(add_symbol(&a,b.plus,op=='+'?1:-1)||add_symbol(&a,b.minus,op=='+'?-1:1))error(c,"unsupported symbolic expression");
            a.value+=op=='+'?b.value:0-b.value;
        } else {
            if(a.plus!=AS_NONE||a.minus!=AS_NONE||b.plus!=AS_NONE||b.minus!=AS_NONE){error(c,"nonconstant arithmetic");break;}
            if((op=='/'||op=='%')&&!b.value){error(c,"division by zero");break;}
            if((op=='<'||op=='>')&&b.value>=64){error(c,"invalid shift");break;}
            switch(op){case '*':a.value*=b.value;break;case '/':a.value/=b.value;break;case '%':a.value%=b.value;break;
            case '<':a.value<<=b.value;break;case '>':a.value>>=b.value;break;case '&':a.value&=b.value;break;case '^':a.value^=b.value;break;case '|':a.value|=b.value;break;}
        }
    }
    return a;
}
static int regname(struct parser *c,char *s,size_t n) {
    blank(c);if(*c->s!='%'){error(c,"expected register");return -1;}c->s++;
    return identifier(c,s,n);
}
static void operand(struct parser *c,struct as_operand *o) {
    memset(o,0,sizeof(*o));o->expression=as_absolute(0);o->scale=1;blank(c);
    if(*c->s=='*'){o->indirect=1;c->s++;blank(c);}
    if(*c->s=='%'){o->kind=AS_REGISTER;regname(c,o->reg,sizeof(o->reg));return;}
    if(*c->s=='$'){o->kind=AS_IMMEDIATE;c->s++;o->expression=expression(c,1);return;}
    o->kind=AS_ADDRESS;
    if(*c->s!='(')o->expression=expression(c,1);
    blank(c);if(*c->s!='(')return;
    o->kind=AS_MEMORY;c->s++;blank(c);
    if(*c->s=='%')regname(c,o->base,sizeof(o->base));
    blank(c);
    if(*c->s==',') {c->s++;blank(c);if(*c->s=='%')regname(c,o->index,sizeof(o->index));blank(c);
        if(*c->s==','){c->s++;struct as_expr scale=expression(c,1);if(scale.plus!=AS_NONE||scale.minus!=AS_NONE||(scale.value!=1&&scale.value!=2&&scale.value!=4&&scale.value!=8))error(c,"invalid address scale");o->scale=(unsigned)scale.value;}}
    blank(c);if(*c->s!=')')error(c,"missing address parenthesis");else c->s++;
}
static int quoted(struct parser *c,unsigned char **data,size_t *length) {
    blank(c);if(*c->s!='"'){error(c,"expected string");return -1;}c->s++;size_t n=0;
    unsigned char *p=malloc(strlen(c->s)+1);if(!p){error(c,"out of memory");return -1;}
    while(*c->s&&*c->s!='"') {
        unsigned ch=(unsigned char)*c->s++;
        if(ch=='\\') {
            ch=(unsigned char)*c->s++;if(!ch){error(c,"unterminated escape");break;}
            if(ch>='0'&&ch<='7'){unsigned k=1;ch-='0';while(k<3&&*c->s>='0'&&*c->s<='7'){ch=ch*8+*c->s++-'0';k++;}}
            else if(ch=='x'){int d=digit(*c->s);if(d<0){error(c,"invalid hex escape");break;}ch=0;while((d=digit(*c->s))>=0){ch=(ch*16+d)&255;c->s++;}}
            else switch(ch){case 'a':ch=7;break;case 'n':ch=10;break;case 't':ch=9;break;case 'r':ch=13;break;case 'b':ch=8;break;case 'f':ch=12;break;case 'v':ch=11;break;}
        }
        p[n++]=(unsigned char)ch;
    }
    if(*c->s!='"'){free(p);error(c,"unterminated string");return -1;}c->s++;*data=p;*length=n;return 0;
}
static size_t symbol_name(struct parser *c) {char name[256];if(identifier(c,name,sizeof(name)))return AS_NONE;size_t i=as_symbol(c->p,name);if(i==AS_NONE)error(c,"out of symbol space");return i;}
static void comma(struct parser *c) {blank(c);if(*c->s!=',')error(c,"expected comma");else c->s++;}
static void directive(struct parser *c,const char *name) {
    if(!strcmp(name,".text")||!strcmp(name,".data")||!strcmp(name,".bss")) {
        c->previous=c->section;c->section=section(c,name,!strcmp(name,".text")?6:3,!strcmp(name,".bss")?8:1);return;
    }
    if(!strcmp(name,".previous")){size_t n=c->section;c->section=c->previous;c->previous=n;return;}
    if(!strcmp(name,".section")) {
        char sectionname[256];blank(c);
        if(*c->s=='"') {
            unsigned char *text;size_t n;if(quoted(c,&text,&n))return;
            if(!n||n>=sizeof(sectionname)){free(text);error(c,"invalid section name");return;}
            for(size_t i=0;i<n;i++)if(!text[i])error(c,"invalid section name");
            memcpy(sectionname,text,n);sectionname[n]=0;free(text);
        } else if(identifier(c,sectionname,sizeof(sectionname)))return;
        uint64_t flags=!strncmp(sectionname,".rodata",7)?2:0;unsigned type=1;blank(c);
        if(*c->s==',') {
            c->s++;unsigned char *text;size_t n;if(quoted(c,&text,&n))return;
            for(size_t i=0;i<n;i++){if(text[i]=='a')flags|=2;else if(text[i]=='w')flags|=1;else if(text[i]=='x')flags|=4;else if(text[i]!='M'&&text[i]!='S')error(c,"unsupported section flags");}
            free(text);blank(c);
            if(*c->s==','){c->s++;blank(c);if(*c->s=='@')c->s++;char typename[32];if(identifier(c,typename,sizeof(typename)))return;if(!strcmp(typename,"nobits"))type=8;else if(strcmp(typename,"progbits"))error(c,"unsupported section type");}
            blank(c);if(*c->s==','){c->s++;struct as_expr e=expression(c,1);if(e.plus!=AS_NONE||e.minus!=AS_NONE||!e.value)error(c,"invalid section entity size");}
        }
        c->previous=c->section;c->section=section(c,sectionname,flags,type);return;
    }
    if(!strcmp(name,".globl")||!strcmp(name,".global")||!strcmp(name,".weak")||!strcmp(name,".local")) {
        do {size_t i=symbol_name(c);if(i==AS_NONE)return;c->p->symbols[i].binding=!strcmp(name,".weak")?2:!strcmp(name,".local")?0:1;blank(c);if(*c->s!=',')break;c->s++;}while(!c->p->errors);return;
    }
    if(!strcmp(name,".type")) {
        size_t i=symbol_name(c);if(i==AS_NONE)return;comma(c);blank(c);if(*c->s=='@'||*c->s=='%')c->s++;
        char type[32];if(identifier(c,type,sizeof(type)))return;
        c->p->symbols[i].type=!strcmp(type,"function")?2:!strcmp(type,"object")?1:0;
        if(strcmp(type,"function")&&strcmp(type,"object")&&strcmp(type,"notype"))error(c,"unsupported symbol type");
        return;
    }
    if(!strcmp(name,".set")||!strcmp(name,".equ")||!strcmp(name,".size")||!strcmp(name,".comm")||!strcmp(name,".lcomm")) {
        size_t i=symbol_name(c);if(i==AS_NONE)return;comma(c);struct as_expr e=expression(c,1);struct as_symbol *s=c->p->symbols+i;
        if(!strcmp(name,".comm")||!strcmp(name,".lcomm")) {
            if(e.plus!=AS_NONE||e.minus!=AS_NONE||s->defined){error(c,"invalid common symbol");return;}
            s->defined=1;s->common=1;s->size=e.value;s->alignment=1;s->binding=!strcmp(name,".comm")?1:0;
            blank(c);if(*c->s==','){c->s++;e=expression(c,1);if(e.plus!=AS_NONE||e.minus!=AS_NONE||!e.value||(e.value&(e.value-1)))error(c,"invalid common alignment");s->alignment=e.value;}
        } else {struct as_record *r=record(c,!strcmp(name,".size")?AS_SIZE:AS_EQUATE);if(!r)return;r->symbol=i;r->expression=e;if(r->kind==AS_EQUATE){if(s->defined)error(c,"duplicate symbol");s->defined=2;s->expression=e;}}
        return;
    }
    if(!strcmp(name,".ascii")||!strcmp(name,".asciz")||!strcmp(name,".string")) {
        do {struct as_record *r=record(c,AS_BYTES);if(!r)return;if(quoted(c,&r->bytes,&r->length))return;
            if(strcmp(name,".ascii"))r->bytes[r->length++]=0;
            blank(c);if(*c->s!=',')break;c->s++;}while(!c->p->errors);return;
    }
    if(!strcmp(name,".float")||!strcmp(name,".double")) {
        do {
            blank(c);char *end;double v=strtod(c->s,&end);if(end==c->s){error(c,"invalid floating constant");return;}c->s=end;
            struct as_record *r=record(c,AS_DATA);if(!r)return;r->width=!strcmp(name,".float")?4:8;
            if(r->width==4){float f=(float)v;uint32_t n;memcpy(&n,&f,4);r->expression=as_absolute(n);}else {uint64_t n;memcpy(&n,&v,8);r->expression=as_absolute(n);}
            blank(c);if(*c->s!=',')break;c->s++;
        }while(!c->p->errors);return;
    }
    unsigned width=!strcmp(name,".byte")?1:!strcmp(name,".word")||!strcmp(name,".short")?2:!strcmp(name,".long")||!strcmp(name,".int")?4:!strcmp(name,".quad")?8:0;
    if(width) {do {struct as_record *r=record(c,AS_DATA);if(!r)return;r->width=width;r->expression=expression(c,1);blank(c);if(*c->s!=',')break;c->s++;}while(!c->p->errors);return;}
    if(!strcmp(name,".zero")||!strcmp(name,".space")||!strcmp(name,".skip")) {
        struct as_record *r=record(c,AS_SPACE);if(!r)return;r->expression=expression(c,1);if(r->expression.plus!=AS_NONE||r->expression.minus!=AS_NONE||r->expression.value>AS_LIMIT)error(c,"invalid space size");
        blank(c);if(*c->s==','){c->s++;struct as_expr e=expression(c,1);if(e.plus!=AS_NONE||e.minus!=AS_NONE||e.value>255)error(c,"invalid fill byte");r->width=(unsigned)e.value;}return;
    }
    if(!strcmp(name,".align")||!strcmp(name,".balign")||!strcmp(name,".p2align")) {
        struct as_record *r=record(c,AS_ALIGN);if(!r)return;r->expression=expression(c,1);
        uint64_t n=r->expression.value;
        if(!strcmp(name,".p2align")){if(n>23){error(c,"alignment too large");return;}n=1UL<<n;r->expression.value=n;}
        if(r->expression.plus!=AS_NONE||r->expression.minus!=AS_NONE||!n||n>AS_LIMIT||(n&(n-1)))error(c,"invalid alignment");
        blank(c);if(*c->s==','){c->s++;struct as_expr e=expression(c,1);if(e.plus!=AS_NONE||e.minus!=AS_NONE||e.value>255)error(c,"invalid alignment fill");r->width=e.value;}return;
    }
    if(!strcmp(name,".file")||!strcmp(name,".ident")) {blank(c);if(!strcmp(name,".file")&&isdigit((unsigned char)*c->s)){struct as_expr e=expression(c,1);if(e.plus!=AS_NONE||e.minus!=AS_NONE)error(c,"invalid file number");}unsigned char *p;size_t n;if(!quoted(c,&p,&n))free(p);return;}
    error(c,"unsupported directive");
}
static void statement(struct parser *c) {
    char name[256];
again:
    blank(c);
    if(!*c->s||*c->s=='#')return;
    if(identifier(c,name,sizeof(name)))return;
    blank(c);
    if(*c->s==':') {
        c->s++;if(strlen(name)==1&&isdigit((unsigned char)*name)){unsigned n=*name-'0';snprintf(name,sizeof(name),".N%u_%u",n,++c->local[n]);}
        size_t i=as_symbol(c->p,name);if(i==AS_NONE){error(c,"out of symbol space");return;}
        struct as_symbol *s=c->p->symbols+i;if(s->defined){error(c,"duplicate symbol");return;}
        s->defined=1;s->section=c->section;s->record=c->p->count;
        struct as_record *r=record(c,AS_LABEL);if(r)r->symbol=i;
        blank(c);if(*c->s&&*c->s!='#'&&*c->s!=';')goto again;return;
    }
    if(*c->s=='=') {
        c->s++;size_t i=as_symbol(c->p,name);if(i==AS_NONE){error(c,"out of symbol space");return;}
        struct as_expr e=expression(c,1);struct as_symbol *s=c->p->symbols+i;if(s->defined)error(c,"duplicate symbol");s->defined=2;s->expression=e;
        struct as_record *r=record(c,AS_EQUATE);if(r){r->symbol=i;r->expression=e;}return;
    }
    if(*name=='.'){directive(c,name);return;}
    if(strlen(name)>=32){error(c,"mnemonic too long");return;}
    struct as_record *r=record(c,AS_INSTRUCTION);if(!r)return;strcpy(r->mnemonic,name);blank(c);
    if(!*c->s||*c->s=='#'||*c->s==';')return;
    do {if(r->count>=3){error(c,"too many operands");return;}operand(c,r->operands+r->count++);blank(c);if(*c->s!=',')break;c->s++;}while(!c->p->errors);
}
int as_parse(const char *path,struct as_program *p) {
    memset(p,0,sizeof(*p));FILE *f=fopen(path,"r");if(!f){fprintf(stderr,"as: cannot read %s\n",path);return -1;}
    struct parser c={0};c.p=p;c.path=path;c.section=section(&c,".text",6,1);c.previous=c.section;
    char line[16384];size_t total=0;
    while(!p->errors) {
        size_t n=0;int ch=EOF;c.line++;
        while(n+1<sizeof(line)&&(ch=fgetc(f))!=EOF) {
            if(!ch){error(&c,"NUL in assembly source");break;}
            if(ch=='\n')break;
            line[n++]=(char)ch;
        }
        if(p->errors||(!n&&ch==EOF))break;
        total+=n+1;line[n]=0;
        if(total>AS_LIMIT||n+1==sizeof(line)){error(&c,"source or line too long");break;}
        c.s=line;
        while(!p->errors) {blank(&c);if(!*c.s||*c.s=='#')break;statement(&c);blank(&c);if(*c.s==';'){c.s++;continue;}if(*c.s&&*c.s!='#')error(&c,"unexpected trailing text");break;}
    }
    if(ferror(f))error(&c,"input read failed");
    if(fclose(f))error(&c,"input close failed");
    return p->errors?-1:0;
}
void as_free(struct as_program *p) {
    for(size_t i=0;i<p->count;i++)free(p->records[i].bytes);
    for(size_t i=0;i<p->symbol_count;i++)free(p->symbols[i].name);
    for(size_t i=0;i<p->section_count;i++)free(p->sections[i].name);
    free(p->records);free(p->symbols);free(p->sections);memset(p,0,sizeof(*p));
}

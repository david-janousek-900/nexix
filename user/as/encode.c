/* x86_64 encodings for the native integer, System V SSE2 and x87 compiler ABI. */
#include "encode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
enum {GP=1,XMM=2,X87=3,RIP=4,R64=1,PC32=2,PLT32=4,R32=10,R32S=11};
struct reg {unsigned n,width,kind,high,force;};
struct encoder {struct as_code *out;unsigned failed;};
static int reg(const char *name,struct reg *r) {
    static const char *names[4][8]={
        {"al","cl","dl","bl","spl","bpl","sil","dil"},
        {"ax","cx","dx","bx","sp","bp","si","di"},
        {"eax","ecx","edx","ebx","esp","ebp","esi","edi"},
        {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi"}};
    memset(r,0,sizeof(*r));
    for(unsigned w=0;w<4;w++)for(unsigned i=0;i<8;i++)if(!strcmp(name,names[w][i])){r->n=i;r->width=1U<<w;r->kind=GP;r->force=w==0&&i>=4;return 0;}
    static const char *high[]={"ah","ch","dh","bh"};
    for(unsigned i=0;i<4;i++)if(!strcmp(name,high[i])){r->n=i+4;r->width=1;r->kind=GP;r->high=1;return 0;}
    if(!strcmp(name,"rip")){r->kind=RIP;r->width=8;return 0;}
    if(name[0]=='r'&&((name[1]>='8'&&name[1]<='9')||name[1]=='1')) {
        char *end;unsigned long n=strtoul(name+1,&end,10);
        if(n>=8&&n<=15) {r->n=n;r->width=8;r->kind=GP;if(!*end)return 0;if(!strcmp(end,"b"))r->width=1;else if(!strcmp(end,"w"))r->width=2;else if(!strcmp(end,"d"))r->width=4;else return -1;return 0;}
    }
    if(!strncmp(name,"xmm",3)&&name[3]) {char *end;unsigned long n=strtoul(name+3,&end,10);if(!*end&&n<16){r->n=n;r->width=16;r->kind=XMM;return 0;}}
    if(!strncmp(name,"st",2)){unsigned n=0;if(name[2]){if(name[2]<'0'||name[2]>'7'||name[3])return -1;n=name[2]-'0';}r->n=n;r->width=10;r->kind=X87;return 0;}
    return -1;
}
static int getreg(const struct as_operand *o,struct reg *r) {return o->kind==AS_REGISTER?reg(o->reg,r):-1;}
static int absolute(struct as_expr e) {return e.plus==AS_NONE&&e.minus==AS_NONE;}
static int signed_fit(uint64_t n,unsigned bits) {int64_t x=(int64_t)n;return bits==64||(x>=-(1LL<<(bits-1))&&x<=(1LL<<(bits-1))-1);}
static void byte(struct encoder *e,unsigned b) {if(e->out->length>=sizeof(e->out->bytes)){e->failed=1;return;}e->out->bytes[e->out->length++]=(unsigned char)b;}
static void number(struct encoder *e,struct as_expr x,unsigned width,unsigned type,int relative) {
    unsigned off=e->out->length;
    if(!absolute(x)||relative) {
        if(width<4){e->failed=1;return;}
        if(e->out->fixup_count>=2){e->failed=1;return;}
        e->out->fixups[e->out->fixup_count++]=(struct as_fixup){off,width,type,x};
        for(unsigned i=0;i<width;i++)byte(e,0);
    } else for(unsigned i=0;i<width;i++)byte(e,(unsigned)(x.value>>(8*i)));
}
struct rm {unsigned char bytes[3];unsigned count,rex,high,force,dispwidth,relative;struct as_expr displacement;};
static int modrm(const struct as_operand *o,unsigned field,struct rm *m) {
    memset(m,0,sizeof(*m));m->displacement=o->expression;
    if(o->kind==AS_REGISTER) {
        struct reg r;if(getreg(o,&r)||r.kind==RIP||r.kind==X87)return -1;
        m->bytes[m->count++]=0xc0|((field&7)<<3)|(r.n&7);m->rex=r.n>=8?1:0;m->high=r.high;m->force=r.force;return 0;
    }
    if(o->kind!=AS_MEMORY&&o->kind!=AS_ADDRESS)return -1;
    struct reg base={0},index={0};int hasbase=*o->base,hasindex=*o->index;
    if(hasbase&&(reg(o->base,&base)||(base.kind!=GP&&base.kind!=RIP)||base.width!=8))return -1;
    if(hasindex&&(reg(o->index,&index)||index.kind!=GP||index.width!=8||index.n==4))return -1;
    if(hasbase&&base.kind==RIP) {
        if(hasindex||(absolute(o->expression)&&!signed_fit(o->expression.value,32)))return -1;
        m->bytes[m->count++]=((field&7)<<3)|5;m->dispwidth=4;m->relative=!absolute(o->expression);return 0;
    }
    unsigned mode=0;
    if(hasbase) {
        if(!absolute(o->expression))mode=2;
        else if(!o->expression.value&&(base.n&7)!=5)mode=0;
        else if(signed_fit(o->expression.value,8))mode=1;
        else mode=2;
    }
    if(absolute(o->expression)&&!signed_fit(o->expression.value,32))return -1;
    int sib=!hasbase||hasindex||(base.n&7)==4;
    m->bytes[m->count++]=(mode<<6)|((field&7)<<3)|(sib?4:(base.n&7));
    if(sib) {
        unsigned scale=o->scale==8?3:o->scale==4?2:o->scale==2?1:0;
        m->bytes[m->count++]=(scale<<6)|((hasindex?index.n&7:4)<<3)|(hasbase?base.n&7:5);
    }
    m->dispwidth=!hasbase||mode==2?4:mode==1?1:0;
    m->rex=(hasbase&&base.n>=8?1:0)|(hasindex&&index.n>=8?2:0);return 0;
}
static int instruction(struct encoder *e,unsigned prefix,unsigned width,const unsigned char *op,unsigned n,
                       unsigned field,unsigned fieldhigh,unsigned fieldforce,const struct as_operand *operand) {
    struct rm m;if(modrm(operand,field,&m))return -1;
    unsigned rex=m.rex|(field>=8?4:0)|(width==8?8:0);
    unsigned high=m.high||fieldhigh,force=m.force||fieldforce;
    if(high&&(rex||force))return -1;
    if(prefix)byte(e,prefix);
    if(width==2&&prefix!=0x66)byte(e,0x66);
    if(rex||force)byte(e,0x40|rex);
    for(unsigned i=0;i<n;i++)byte(e,op[i]);
    for(unsigned i=0;i<m.count;i++)byte(e,m.bytes[i]);
    if(m.dispwidth)number(e,m.displacement,m.dispwidth,m.relative?PC32:R32S,m.relative);
    return e->failed?-1:0;
}
static int group(struct encoder *e,unsigned prefix,unsigned width,unsigned op,unsigned field,const struct as_operand *o) {
    unsigned char bytes[2];unsigned n=1;bytes[0]=op;
    if(op>255){bytes[0]=op>>8;bytes[1]=op;n=2;}
    return instruction(e,prefix,width,bytes,n,field,0,0,o);
}
static int pair(struct encoder *e,unsigned prefix,unsigned width,unsigned op,struct reg r,const struct as_operand *o) {
    unsigned char bytes[2];unsigned n=1;bytes[0]=op;
    if(op>255){bytes[0]=op>>8;bytes[1]=op;n=2;}
    return instruction(e,prefix,width,bytes,n,r.n,r.high,r.force,o);
}
static unsigned suffix(char *name) {
    size_t n=strlen(name);if(!n)return 0;unsigned width=name[n-1]=='b'?1:name[n-1]=='w'?2:name[n-1]=='l'?4:name[n-1]=='q'?8:0;
    if(width)name[n-1]=0;
    return width;
}
static int gpwidth(const struct as_operand *o,unsigned width) {struct reg r;return o->kind!=AS_REGISTER||(!getreg(o,&r)&&r.kind==GP&&r.width==width);}
static int condition(const char *s) {
    static const char *names[]={"o","no","b","c","nae","ae","nb","nc","e","z","ne","nz","be","na","a","nbe","s","ns","p","pe","np","po","l","nge","ge","nl","le","ng","g","nle"};
    static const int codes[]={0,1,2,2,2,3,3,3,4,4,5,5,6,6,7,7,8,9,10,10,11,11,12,12,13,13,14,14,15,15};
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++)if(!strcmp(s,names[i]))return codes[i];
    return -1;
}
static int encode(const struct as_record *r,struct encoder *e) {
    const struct as_operand *a=r->operands,*b=a+1,*c=a+2;struct reg x={0},y={0};
    int ar=!getreg(a,&x),br=!getreg(b,&y);char name[32];strcpy(name,r->mnemonic);
    static const struct {const char *name;unsigned code,n;} simple[]={
        {"int3",0xcc,1},{"movsb",0xa4,1},{"movsw",0x66a5,2},{"movsl",0xa5,1},{"movsq",0x48a5,2},
        {"rep_movsb",0xf3a4,2},{"rep_movsq",0xf348a5,3},{"rep_stosb",0xf3aa,2},{"rep_stosq",0xf348ab,3},
        {"nop",0x90,1},{"ret",0xc3,1},{"retq",0xc3,1},{"leave",0xc9,1},{"leaveq",0xc9,1},
        {"syscall",0x0f05,2},{"ud2",0x0f0b,2},{"cld",0xfc,1},{"std",0xfd,1},
        {"cqto",0x4899,2},{"cqo",0x4899,2},{"cltq",0x4898,2},{"cdqe",0x4898,2},
        {"cltd",0x99,1},{"cdq",0x99,1},{"cwtl",0x98,1},{"pushfq",0x9c,1},{"popfq",0x9d,1},
        {"fchs",0xd9e0,2},{"fabs",0xd9e1,2},{"ftst",0xd9e4,2},{"fld1",0xd9e8,2},{"fldz",0xd9ee,2},{"fsqrt",0xd9fa,2},{"fninit",0xdbe3,2},{"fwait",0x9b,1}};
    for(unsigned i=0;i<sizeof(simple)/sizeof(*simple);i++)if(!strcmp(name,simple[i].name)&&!r->count) {
        for(unsigned j=simple[i].n;j;j--)byte(e,simple[i].code>>(8*(j-1)));
        return 0;
    }
    /* Scalar/packed SSE2 movement and arithmetic. */
    static const struct {const char *name;unsigned prefix,load,store;} sse[]={
        {"movss",0xf3,0x10,0x11},{"movsd",0xf2,0x10,0x11},{"movups",0,0x10,0x11},{"movaps",0,0x28,0x29},
        {"movupd",0x66,0x10,0x11},{"movapd",0x66,0x28,0x29},{"movdqa",0x66,0x6f,0x7f},{"movdqu",0xf3,0x6f,0x7f},
        {"addss",0xf3,0x58,0},{"addsd",0xf2,0x58,0},{"subss",0xf3,0x5c,0},{"subsd",0xf2,0x5c,0},
        {"mulss",0xf3,0x59,0},{"mulsd",0xf2,0x59,0},{"divss",0xf3,0x5e,0},{"divsd",0xf2,0x5e,0},
        {"sqrtss",0xf3,0x51,0},{"sqrtsd",0xf2,0x51,0},{"ucomiss",0,0x2e,0},{"ucomisd",0x66,0x2e,0},
        {"comiss",0,0x2f,0},{"comisd",0x66,0x2f,0},{"xorps",0,0x57,0},{"xorpd",0x66,0x57,0},{"pxor",0x66,0xef,0},
        {"cvtss2sd",0xf3,0x5a,0},{"cvtsd2ss",0xf2,0x5a,0}};
    for(unsigned i=0;i<sizeof(sse)/sizeof(*sse);i++)if(!strcmp(name,sse[i].name)&&r->count==2) {
        if(br&&y.kind==XMM&&(a->kind!=AS_REGISTER||(ar&&x.kind==XMM)))return pair(e,sse[i].prefix,0,0x0f00|sse[i].load,y,a);
        if(ar&&x.kind==XMM&&sse[i].store&&b->kind!=AS_REGISTER)return pair(e,sse[i].prefix,0,0x0f00|sse[i].store,x,b);
        return -1;
    }
    if((!strcmp(name,"movq")||!strcmp(name,"movd"))&&r->count==2&&((ar&&x.kind==XMM)||(br&&y.kind==XMM))) {
        unsigned w=!strcmp(name,"movq")?8:4;
        if(ar&&x.kind==XMM&&br&&y.kind==XMM&&w==8)return pair(e,0xf3,0,0x0f7e,y,a);
        if(w==8&&br&&y.kind==XMM&&a->kind!=AS_REGISTER)return pair(e,0xf3,0,0x0f7e,y,a);
        if(w==8&&ar&&x.kind==XMM&&b->kind!=AS_REGISTER)return pair(e,0x66,0,0x0fd6,x,b);
        if(br&&y.kind==XMM&&(!ar||(x.kind==GP&&x.width==w)))return pair(e,0x66,w==8?8:0,0x0f6e,y,a);
        if(ar&&x.kind==XMM&&(!br||(y.kind==GP&&y.width==w)))return pair(e,0x66,w==8?8:0,0x0f7e,x,b);
        return -1;
    }
    static const struct {const char *name;unsigned prefix,op,tofloat;} conversions[]={
        {"cvtsi2ss",0xf3,0x2a,1},{"cvtsi2sd",0xf2,0x2a,1},
        {"cvttss2si",0xf3,0x2c,0},{"cvttsd2si",0xf2,0x2c,0}};
    for(unsigned i=0;i<4;i++) {
        size_t n=strlen(conversions[i].name);unsigned w=4;
        if(strncmp(name,conversions[i].name,n))continue;
        if(!name[n]&&ar&&x.kind==GP)w=x.width;
        if(name[n]=='q'&&!name[n+1])w=8;else if(name[n]&&!(name[n]=='l'&&!name[n+1]))continue;
        if(r->count!=2)return -1;
        if(conversions[i].tofloat) {if(!br||y.kind!=XMM||!gpwidth(a,w))return -1;return pair(e,conversions[i].prefix,w==8?8:0,0x0f00|conversions[i].op,y,a);}
        if(!br||y.kind!=GP||(y.width!=4&&y.width!=8)||(name[n]&&y.width!=w)||(ar&&x.kind!=XMM))return -1;
        return pair(e,conversions[i].prefix,y.width==8?8:0,0x0f00|conversions[i].op,y,a);
    }
    static const struct {const char *name;unsigned op,ext;} fp[]={
        {"flds",0xd9,0},{"fldl",0xdd,0},{"fldt",0xdb,5},{"fsts",0xd9,2},{"fstps",0xd9,3},
        {"fstl",0xdd,2},{"fstpl",0xdd,3},{"fstpt",0xdb,7},{"filds",0xdf,0},{"fildl",0xdb,0},
        {"fildq",0xdf,5},{"fistps",0xdf,3},{"fistpl",0xdb,3},{"fistpq",0xdf,7},{"fnstcw",0xd9,7},{"fldcw",0xd9,5}};
    for(unsigned i=0;i<sizeof(fp)/sizeof(*fp);i++)if(!strcmp(name,fp[i].name)&&r->count==1&&a->kind!=AS_REGISTER)return group(e,0,0,fp[i].op,fp[i].ext,a);
    if(!strcmp(name,"fnstsw")&&r->count==1&&ar&&!strcmp(a->reg,"ax")){byte(e,0xdf);byte(e,0xe0);return 0;}
    if(ar&&x.kind==X87) {
        unsigned code=0;
        if(!strcmp(name,"fld")&&r->count==1)code=0xd9c0;
        if(!strcmp(name,"fxch")&&r->count==1)code=0xd9c8;
        if(!strcmp(name,"fstp")&&r->count==1)code=0xddd8;
        if((r->count==1||(r->count==2&&br&&y.kind==X87&&y.n==0))) {
            if(!strcmp(name,"fucomip"))code=0xdfe8;
            if(!strcmp(name,"fcomip"))code=0xdff0;
            if(!strcmp(name,"fucomi"))code=0xdbe8;
            if(!strcmp(name,"fcomi"))code=0xdbf0;
        }
        if(code){byte(e,code>>8);byte(e,code|x.n);return 0;}
    }
    static const struct {const char *name;unsigned code;} popfp[]={
        {"faddp",0xdec0},{"fmulp",0xdec8},{"fsubp",0xdee0},{"fsubrp",0xdee8},{"fdivp",0xdef0},{"fdivrp",0xdef8}};
    for(unsigned i=0;i<6;i++)if(!strcmp(name,popfp[i].name)) {
        unsigned n=1;if(r->count==1){if(!ar||x.kind!=X87)return -1;n=x.n;}
        else if(r->count==2){if(!ar||!br||x.kind!=X87||x.n||y.kind!=X87)return -1;n=y.n;}
        else if(r->count)return -1;
        byte(e,popfp[i].code>>8);byte(e,popfp[i].code|n);return 0;
    }
    if(name[0]=='j'&&strcmp(name,"jmp")&&r->count==1) {
        int cc=condition(name+1);if(cc<0||a->kind!=AS_ADDRESS||a->indirect)return -1;
        byte(e,0x0f);byte(e,0x80|cc);number(e,a->expression,4,PLT32,1);return 0;
    }
    if(!strncmp(name,"set",3)&&r->count==1) {
        int cc=condition(name+3);if(cc<0||!gpwidth(a,1))return -1;
        return group(e,0,1,0x0f90|cc,0,a);
    }
    if(!strncmp(name,"cmov",4)&&r->count==2) {
        int cc=condition(name+4);if(cc<0){suffix(name);cc=condition(name+4);}
        if(cc<0||!br||y.kind!=GP||y.width==1||!gpwidth(a,y.width))return -1;
        return pair(e,0,y.width,0x0f40|cc,y,a);
    }
    if(!strcmp(name,"movslq")&&r->count==2&&br&&y.kind==GP&&y.width==8&&gpwidth(a,4))return pair(e,0,8,0x63,y,a);
    if((!strncmp(name,"movz",4)||!strncmp(name,"movs",4))&&strlen(name)==6&&r->count==2) {
        unsigned source=name[4]=='b'?1:name[4]=='w'?2:0,target=name[5]=='l'?4:name[5]=='q'?8:name[5]=='w'?2:0;
        if(!source||!target||!br||y.kind!=GP||y.width!=target||!gpwidth(a,source))return -1;
        return pair(e,0,target,0x0f00|(name[3]=='z'?0xb6:0xbe)|(source==2),y,a);
    }
    static const char *core[]={"call","jmp","push","pop","mov","movabs","lea","add","or","adc","sbb","and","sub","xor","cmp","test","imul","inc","dec","not","neg","mul","div","idiv","rol","ror","rcl","rcr","shl","shr","sal","sar","bsf","bsr","xchg"};
    unsigned requested=0;int full=0;
    for(unsigned i=0;i<sizeof(core)/sizeof(*core);i++)if(!strcmp(name,core[i]))full=1;
    if(!full)requested=suffix(name);
    unsigned width=requested;if(!width){if(br&&y.kind==GP)width=y.width;else if(ar&&x.kind==GP)width=x.width;else width=4;}
    if(!strcmp(name,"call")||!strcmp(name,"jmp")) {
        if(r->count!=1)return -1;
        if(a->indirect){if(ar&&(x.kind!=GP||x.width!=8))return -1;return group(e,0,0,0xff,!strcmp(name,"call")?2:4,a);}
        if(a->kind!=AS_ADDRESS)return -1;
        byte(e,!strcmp(name,"call")?0xe8:0xe9);number(e,a->expression,4,PLT32,1);return 0;
    }
    if(!strcmp(name,"push")||!strcmp(name,"pop")) {
        if(r->count!=1)return -1;
        unsigned w=requested?requested:ar?x.width:8;if((w!=2&&w!=8)||(ar&&(x.kind!=GP||x.width!=w)))return -1;
        if(ar&&x.kind==GP){if(w==2)byte(e,0x66);if(x.n>=8)byte(e,0x41);byte(e,(!strcmp(name,"push")?0x50:0x58)|(x.n&7));return 0;}
        if(a->kind==AS_IMMEDIATE&&!strcmp(name,"push")) {
            unsigned n=absolute(a->expression)&&signed_fit(a->expression.value,8)?1:w==2?2:4;
            if(absolute(a->expression)&&!signed_fit(a->expression.value,n*8))return -1;
            if(w==2)byte(e,0x66);
            byte(e,n==1?0x6a:0x68);number(e,a->expression,n,R32S,0);return 0;
        }
        return group(e,0,w==2?2:0,!strcmp(name,"push")?0xff:0x8f,!strcmp(name,"push")?6:0,a);
    }
    if(!strcmp(name,"mov")||!strcmp(name,"movabs")) {
        if(r->count!=2||!gpwidth(b,width))return -1;
        if(a->kind==AS_IMMEDIATE) {
            if(br&&y.kind==GP) {
                unsigned n=width;int short64=width==8&&strcmp(name,"movabs")&&(!absolute(a->expression)||signed_fit(a->expression.value,32));
                if(short64){if(group(e,0,8,0xc7,0,b))return -1;n=4;}
                else {if(width==2)byte(e,0x66);unsigned rex=(width==8?8:0)|(y.n>=8?1:0);if(y.high&&(rex||y.force))return -1;if(rex||y.force)byte(e,0x40|rex);byte(e,(width==1?0xb0:0xb8)|(y.n&7));}
                number(e,a->expression,n,n==8?R64:short64?R32S:R32,0);return 0;
            }
            if(width==8&&absolute(a->expression)&&!signed_fit(a->expression.value,32))return -1;
            if(group(e,0,width,width==1?0xc6:0xc7,0,b))return -1;
            number(e,a->expression,width==8?4:width,width==8?R32S:R32,0);return 0;
        }
        if(!gpwidth(a,width))return -1;
        if(ar&&x.kind==GP)return pair(e,0,width,width==1?0x88:0x89,x,b);
        if(br&&y.kind==GP)return pair(e,0,width,width==1?0x8a:0x8b,y,a);
        return -1;
    }
    if(!strcmp(name,"lea")) {if(r->count!=2||!br||y.kind!=GP||width==1||y.width!=width||a->kind==AS_REGISTER)return -1;return pair(e,0,width,0x8d,y,a);}
    static const char *binary[]={"add","or","adc","sbb","and","sub","xor","cmp"};
    for(unsigned op=0;op<8;op++)if(!strcmp(name,binary[op])) {
        if(r->count!=2||!gpwidth(b,width))return -1;
        if(a->kind==AS_IMMEDIATE) {
            unsigned n=width==8?4:width;int small=width>1&&absolute(a->expression)&&signed_fit(a->expression.value,8);
            if(width==8&&absolute(a->expression)&&!signed_fit(a->expression.value,32))return -1;
            if(br&&y.kind==GP&&y.n==0&&!small) {if(width==2)byte(e,0x66);if(width==8)byte(e,0x48);byte(e,op*8+(width==1?4:5));}
            else if(group(e,0,width,width==1?0x80:small?0x83:0x81,op,b))return -1;
            number(e,a->expression,small?1:n,width==8?R32S:R32,0);return 0;
        }
        if(!gpwidth(a,width))return -1;
        if(ar&&x.kind==GP)return pair(e,0,width,op*8+(width==1?0:1),x,b);
        if(br&&y.kind==GP)return pair(e,0,width,op*8+(width==1?2:3),y,a);
        return -1;
    }
    if(!strcmp(name,"test")) {
        if(r->count!=2||!gpwidth(b,width))return -1;
        if(a->kind==AS_IMMEDIATE) {
            if(width==8&&absolute(a->expression)&&!signed_fit(a->expression.value,32))return -1;
            if(br&&y.kind==GP&&y.n==0){if(width==2)byte(e,0x66);if(width==8)byte(e,0x48);byte(e,width==1?0xa8:0xa9);}
            else if(group(e,0,width,width==1?0xf6:0xf7,0,b))return -1;
            number(e,a->expression,width==8?4:width,width==8?R32S:R32,0);return 0;
        }
        if(!ar||x.kind!=GP||x.width!=width)return -1;
        return pair(e,0,width,width==1?0x84:0x85,x,b);
    }
    static const struct {const char *name;unsigned field;} unary[]={ {"inc",0},{"dec",1},{"not",2},{"neg",3},{"mul",4},{"imul",5},{"div",6},{"idiv",7}};
    if(!strcmp(name,"imul")&&r->count>=2) {
        if(r->count==2&&br&&y.kind==GP&&y.width==width&&gpwidth(a,width)&&a->kind!=AS_IMMEDIATE)return pair(e,0,width,0x0faf,y,a);
        if(a->kind==AS_IMMEDIATE) {
            const struct as_operand *source=b,*dest=r->count==3?c:b;struct reg d;
            if(getreg(dest,&d)||d.kind!=GP||d.width!=width||width==1||!gpwidth(source,width))return -1;
            unsigned n=absolute(a->expression)&&signed_fit(a->expression.value,8)?1:width==2?2:4;
            if(width==8&&absolute(a->expression)&&!signed_fit(a->expression.value,32))return -1;
            if(pair(e,0,width,n==1?0x6b:0x69,d,source))return -1;
            number(e,a->expression,n,width==8?R32S:R32,0);return 0;
        }
        return -1;
    }
    for(unsigned i=0;i<8;i++)if(!strcmp(name,unary[i].name)&&r->count==1) {
        if(!gpwidth(a,width))return -1;
        unsigned op=i<2?(width==1?0xfe:0xff):(width==1?0xf6:0xf7);
        return group(e,0,width,op,unary[i].field,a);
    }
    static const char *shifts[]={"rol","ror","rcl","rcr","shl","shr","sal","sar"};
    for(unsigned i=0;i<8;i++)if(!strcmp(name,shifts[i])&&(r->count==1||r->count==2)) {
        const struct as_operand *dest=r->count==1?a:b;if(!gpwidth(dest,width))return -1;
        unsigned field=i==6?4:i;
        if(r->count==1||(a->kind==AS_IMMEDIATE&&absolute(a->expression)&&a->expression.value==1))return group(e,0,width,width==1?0xd0:0xd1,field,dest);
        if(ar&&x.kind==GP&&!strcmp(a->reg,"cl"))return group(e,0,width,width==1?0xd2:0xd3,field,dest);
        if(a->kind!=AS_IMMEDIATE||!absolute(a->expression)||a->expression.value>255)return -1;
        if(group(e,0,width,width==1?0xc0:0xc1,field,dest))return -1;
        number(e,a->expression,1,R32,0);return 0;
    }
    if((!strcmp(name,"bsf")||!strcmp(name,"bsr"))&&r->count==2&&br&&y.kind==GP&&y.width==width&&width>1&&gpwidth(a,width))return pair(e,0,width,!strcmp(name,"bsf")?0x0fbc:0x0fbd,y,a);
    if(!strcmp(name,"xchg")&&r->count==2&&ar&&x.kind==GP&&gpwidth(b,width)&&x.width==width) {
        if(br&&y.kind==GP&&width>1&&(!x.n||!y.n)&&!(width==4&&!x.n&&!y.n)) {
            unsigned n=x.n?x.n:y.n;
            if(width==2)byte(e,0x66);
            unsigned rex=(width==8&&n?8:0)|(n>=8?1:0);if(rex)byte(e,0x40|rex);
            byte(e,0x90|(n&7));return 0;
        }
        return pair(e,0,width,width==1?0x86:0x87,x,b);
    }
    return -1;
}
int as_encode(const struct as_record *r,struct as_code *out) {
    memset(out,0,sizeof(*out));if(r->kind!=AS_INSTRUCTION)return -1;
    struct encoder e={out,0};if(encode(r,&e)||e.failed||out->length>15)return -1;
    for(unsigned i=0;i<out->fixup_count;i++)if(out->fixups[i].type==PC32||out->fixups[i].type==PLT32)
        out->fixups[i].expression.value-=out->length-out->fixups[i].offset;
    return 0;
}

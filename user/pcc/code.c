/* Native ELF/AMD64 equivalents of the supplied PCC code.c output hooks. */
#include "backend.h"
static int location=PROG,label_counter=10;
static unsigned frame_size;
static uint32_t initial_word;
static unsigned initial_bits;
int getlab(void) {if(label_counter==0x3fffffff)cerror("too many parser labels");return ++label_counter;}
void pcc_note_frame(unsigned bytes) {if(bytes>frame_size)frame_size=bytes;}
unsigned pcc_frame_size(void) {return frame_size;}
int locctr(int n) {
    int old=location;if(n==location)return old;location=n;
    if(n==PROG)puts(".text");
    else if(n==DATA || n==ADATA)puts(".data");
    else if(n==STRNG || n==ISTRNG)puts(".section .rodata");
    else cerror("invalid output section");return old;
}
int defalign(int bits) {
    if(bits<8 || bits%8)cerror("invalid object alignment");
    if(bits>8)printf(".balign %u\n",(unsigned)bits/8);return 0;
}
int deflab(int n) {printf(".L%d:\n",n);return 0;}
int branch(int n) {printf(" jmp .L%d\n",n);return 0;}
static const char *symbol_name(struct symtab *p,char name[32]) {
    if(p->sclass==STATIC && p->slevel>1) {snprintf(name,32,".L%d",p->offset);return name;}
    return p->sname;
}
int defnam(struct symtab *p) {
    char local[32];const char *name=symbol_name(p,local);
    if(p->sclass==EXTDEF)printf(".globl %s\n",name);
    printf(".type %s,@object\n%s:\n",name,name);return 0;
}
static int result_hidden,result_temp,result_end;
static unsigned result_class;
static int frame_slot(unsigned bytes,unsigned alignment) {
    unsigned bits=alignment*8;autooff=(autooff+(int)bits-1)/(int)bits*(int)bits;
    if((unsigned)autooff>INT32_MAX/2-bytes*8)cerror("frame too large");
    autooff+=(int)(bytes*8);return -autooff/8;
}
static unsigned aggregate_need(unsigned c,unsigned class) {return (PCC_CLASS_AT(c,0)==class)+(PCC_CLASS_AT(c,1)==class);}
static int register_class(unsigned c) {unsigned a=PCC_CLASS_AT(c,0);return a==PCC_CLASS_INTEGER || a==PCC_CLASS_SSE;}
int bfcode(int *args,int count) {
    struct symtab *function=stab+curftn;TWORD result=DECREF(function->stype);
    static const char *gp[]={"rdi","rsi","rdx","rcx","r8","r9"};int i;unsigned ngp=0,nsse=0,stack=16;
    frame_size=0;autooff=0;locctr(PROG);result_hidden=result_temp=0;result_class=PCC_CLASS_NONE;
    if(function->sclass==EXTDEF)printf(".globl %s\n",function->sname);
    printf(".type %s,@function\n%s:\n pushq %%rbp\n movq %%rsp,%%rbp\n subq $.F%d,%%rsp\n",function->sname,function->sname,ftnno);
    retlab=getlab();
    if(result==STRTY || result==UNIONTY) {
        result_class=pcc_classify(function->sizoff);result_end=getlab();
        if(PCC_CLASS_AT(result_class,0)==PCC_CLASS_MEMORY) {result_hidden=frame_slot(8,8);printf(" movq %%rdi,%d(%%rbp)\n",result_hidden);ngp=1;}
        else if(PCC_CLASS_AT(result_class,0)!=PCC_CLASS_X87)result_temp=frame_slot(16,16);
    }
    if(pcc_definition_variadic())pcc_va_frame.save=frame_slot(176,16);
    for(i=0;i<count;i++) {
        struct symtab *p=stab+args[i];p->sclass=PARAM;
        if(p->stype==LDOUBLE) {stack=(stack+15)&~15U;p->offset=(int)stack*8;stack+=16;continue;}
        if(p->stype==STRTY || p->stype==UNIONTY) {
            unsigned c=pcc_classify(p->sizoff),bytes=(unsigned)(tsize(p->stype,p->dimoff,p->sizoff)/8),align=(unsigned)talign(p->stype,p->sizoff)/8;
            unsigned needgp=aggregate_need(c,PCC_CLASS_INTEGER),needsse=aggregate_need(c,PCC_CLASS_SSE),j;
            if(!register_class(c) || ngp+needgp>6 || nsse+needsse>8) {
                if(align>8)stack=(stack+15)&~15U;
                p->offset=(int)stack*8;stack+=(bytes+7)&~7U;continue;
            }
            p->offset=frame_slot(16,align>8?16:8)*8;
            for(j=0;j<2 && PCC_CLASS_AT(c,j)!=PCC_CLASS_NONE;j++) {
                if(PCC_CLASS_AT(c,j)==PCC_CLASS_SSE)printf(" movsd %%xmm%u,%d(%%rbp)\n",nsse++,p->offset/8+(int)j*8);
                else printf(" movq %%%s,%d(%%rbp)\n",gp[ngp++],p->offset/8+(int)j*8);
            }
            continue;
        }
        autooff+=64;p->offset=-autooff;
        if((p->stype==FLOAT || p->stype==DOUBLE) && nsse<8)printf(" mov%s %%xmm%u,%d(%%rbp)\n",p->stype==FLOAT?"ss":"sd",nsse++,p->offset/8);
        else if(p->stype!=FLOAT && p->stype!=DOUBLE && ngp<6)printf(" movq %%%s,%d(%%rbp)\n",gp[ngp++],p->offset/8);
        else {printf(" movq %u(%%rbp),%%rax\n movq %%rax,%d(%%rbp)\n",stack,p->offset/8);stack+=8;}
    }
    if(pcc_definition_variadic()) {
        /* Every incoming register is saved; va_arg consumes those after the named ones. */
        unsigned j;for(j=0;j<6;j++)printf(" movq %%%s,%d(%%rbp)\n",gp[j],pcc_va_frame.save+(int)j*8);
        for(j=0;j<8;j++)printf(" movsd %%xmm%u,%d(%%rbp)\n",j,pcc_va_frame.save+48+(int)j*16);
        pcc_va_frame.gp=ngp*8;pcc_va_frame.fp=48+nsse*16;pcc_va_frame.overflow=stack;
    }
    pcc_note_frame((unsigned)autooff/8);return 0;
}
int bccode(void) {pcc_note_frame((unsigned)autooff/8);return 0;}
int efcode(void) {
    if(result_class!=PCC_CLASS_NONE) {
        /* Returns arrive with the value address in RAX; falling off the end skips the copy. */
        branch(result_end);deflab(retlab);unsigned a=PCC_CLASS_AT(result_class,0);
        unsigned long bytes=(unsigned long)(tsize(STRTY,0,stab[curftn].sizoff)/8);
        if(a==PCC_CLASS_X87)puts(" fldt (%rax)");
        else if(a==PCC_CLASS_MEMORY)printf(" movq %%rax,%%rsi\n movq %d(%%rbp),%%rdi\n movq $%lu,%%rcx\n cld\n rep movsb\n movq %d(%%rbp),%%rax\n",result_hidden,bytes,result_hidden);
        else {
            unsigned j,ngp=0,nsse=0;static const char *gp[]={"rax","rdx"};
            printf(" movq %%rax,%%rsi\n leaq %d(%%rbp),%%rdi\n movq $%lu,%%rcx\n cld\n rep movsb\n",result_temp,bytes);
            for(j=0;j<2 && PCC_CLASS_AT(result_class,j)!=PCC_CLASS_NONE;j++) {
                if(PCC_CLASS_AT(result_class,j)==PCC_CLASS_SSE)printf(" movsd %d(%%rbp),%%xmm%u\n",result_temp+(int)j*8,nsse++);
                else printf(" movq %d(%%rbp),%%%s\n",result_temp+(int)j*8,gp[ngp++]);
            }
        }
        deflab(result_end);
    } else deflab(retlab);
    printf(" leave\n ret\n.set .F%d,%u\n.size %s,.-%s\n",ftnno,(frame_size+15)&~15U,stab[curftn].sname,stab[curftn].sname);return 0;
}
int aobeg(void) {return 0;}
int aocode(struct symtab *p) {if(p->sclass==AUTO && p->offset<0)pcc_note_frame((unsigned)(-(int64_t)p->offset)/8);return 0;}
int aoend(void) {return 0;}
int ejobcode(int bad) {(void)bad;puts(".section .note.GNU-stack,\"\",@progbits");if(fflush(stdout))uerror("assembly output failed");return 0;}
int genswitch(struct sw *p,int count) {
    int i;for(i=1;i<=count;i++)printf(" movq $%ld,%%rcx\n cmpq %%rcx,%%rax\n je .L%d\n",(long)p[i].sval,p[i].slab);
    if(p[0].slab>=0)branch(p[0].slab);return 0;
}
int fldal(TWORD t) {(void)t;return ALINT;}
int fldty(struct symtab *p) {(void)p;return 0;}
int commdec(int id) {
    struct symtab *p=stab+id;OFFSZ size=tsize(p->stype,p->dimoff,p->sizoff);
    printf(".comm %s,%lu,%u\n",p->sname,(unsigned long)size/8,(unsigned)talign(p->stype,p->sizoff)/8);return 0;
}
int bycode(int c,int i) {(void)i;if(c>=0)printf(".byte %u\n",(unsigned)c&255);return 0;}
int zecode(int words) {if(words<0)cerror("negative initialization size");if(words)printf(".zero %lu\n",(unsigned long)words*4);inoff+=(OFFSZ)words*32;return 0;}
static void flush_word(void) {printf(".long 0x%x\n",initial_word);initial_word=initial_bits=0;}
int incode(NODE *p,int bits) {
    if(bits<=0 || (unsigned)bits>32-initial_bits)cerror("invalid initializer bit field");
    uint32_t mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1;
    initial_word|=((uint32_t)p->lval&mask)<<initial_bits;initial_bits+=(unsigned)bits;inoff+=bits;
    if(initial_bits==32)flush_word();return 0;
}
int vfdzero(int bits) {
    if(bits<0)cerror("negative initialization padding");
    inoff+=bits;
    if(initial_bits) {
        unsigned n=(unsigned)bits<32-initial_bits?(unsigned)bits:32-initial_bits;
        initial_bits+=n;bits-=(int)n;if(initial_bits==32)flush_word();
    }
    if(bits>=32) {printf(".zero %u\n",(unsigned)bits/32*4);bits%=32;}
    initial_bits+=(unsigned)bits;return 0;
}
int fincode(long double value,int bits) {
    if(bits==32) {float f=(float)value;uint32_t v;memcpy(&v,&f,4);printf(".long %u\n",v);}
    else if(bits==64) {uint64_t v;double d=(double)value;memcpy(&v,&d,8);printf(".quad %lu\n",(unsigned long)v);}
    else if(bits==128) {uint64_t lo;unsigned short hi;memcpy(&lo,&value,8);memcpy(&hi,(char *)&value+8,2);printf(".quad %lu\n.short %u\n.zero 6\n",(unsigned long)lo,(unsigned)hi);}
    else cerror("invalid floating initializer width");
    inoff+=bits;return 0;
}
int ecode(NODE *tree) {
    if(nerrors)return 0;
    unsigned capacity=pcc_ir_capacity(tree);
    struct mip_node *nodes=calloc(capacity,sizeof(*nodes));if(!nodes)cerror("no memory for target IR");
    struct mip_arena arena={nodes,0,capacity};uint32_t root=pcc_lower(tree,&arena);
    if(pcc_output_tree(&arena,root,0))cerror("expression pass output failed");
    free(nodes);return 0;
}
int cinit(NODE *tree,int bits) {
    if(nerrors)return 0;
    if(initial_bits)cerror("unaligned scalar initializer");
    unsigned capacity=pcc_ir_capacity(tree);
    struct mip_node *nodes=calloc(capacity,sizeof(*nodes));if(!nodes)cerror("no memory for initializer IR");
    struct mip_arena arena={nodes,0,capacity};uint32_t root=pcc_lower(tree,&arena);
    if(pcc_output_tree(&arena,root,(unsigned)bits))cerror("initializer pass output failed");
    inoff+=bits;free(nodes);return 0;
}
int main(int argc,char **argv) {
    unsigned emit=0,read=0;int source=1;
    if(argc>1 && !strcmp(argv[1],"--emit-pass")) {emit=1;source++;}
    else if(argc>1 && !strcmp(argv[1],"--read-pass")) {read=1;source++;}
    if(argc>source && !strcmp(argv[source],"--native-source")) {pcc_native_source=1;source++;}
    if(argc>source+1) {fprintf(stderr,"usage: pcc-backend [--emit-pass|--read-pass] [input]\n");return 2;}
    if(argc>source && strcmp(argv[source],"-")) {if(!freopen(argv[source],"r",stdin))return 1;snprintf(ftitle,sizeof(ftitle),"%s",argv[source]);}
    if(read) {
        if(pcc_read_pass(stdin)) {fprintf(stderr,"pcc: invalid or failed compiler pass\n");return 1;}
        return 0;
    }
    if(emit && pcc_output_begin())return 1;
    puts(".text");int status=mainp1(1,argv);
    if(!status && pcc_output_end())status=1;
    return status;
}

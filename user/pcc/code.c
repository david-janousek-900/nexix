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
int bfcode(int *args,int count) {
    struct symtab *function=stab+curftn;TWORD result=DECREF(function->stype);struct mip_layout layout;
    if(mip_scalar_layout(result,&layout)) {uerror("function result requires aggregate backend");return 0;}
    frame_size=0;autooff=0;locctr(PROG);
    if(function->sclass==EXTDEF)printf(".globl %s\n",function->sname);
    printf(".type %s,@function\n%s:\n pushq %%rbp\n movq %%rsp,%%rbp\n subq $.F%d,%%rsp\n",function->sname,function->sname,ftnno);
    retlab=getlab();static const char *gp[]={"rdi","rsi","rdx","rcx","r8","r9"};int i;unsigned ngp=0,nsse=0,stack=16;
    for(i=0;i<count;i++) {
        struct symtab *p=stab+args[i];
        if(mip_scalar_layout(p->stype,&layout)) {uerror("parameter requires aggregate backend");continue;}
        autooff+=64;p->offset=-autooff;p->sclass=PARAM;
        if((p->stype==FLOAT || p->stype==DOUBLE) && nsse<8)printf(" mov%s %%xmm%u,%d(%%rbp)\n",p->stype==FLOAT?"ss":"sd",nsse++,p->offset/8);
        else if(p->stype!=FLOAT && p->stype!=DOUBLE && ngp<6)printf(" movq %%%s,%d(%%rbp)\n",gp[ngp++],p->offset/8);
        else {printf(" movq %u(%%rbp),%%rax\n movq %%rax,%d(%%rbp)\n",stack,p->offset/8);stack+=8;}
    }
    pcc_note_frame((unsigned)autooff/8);return 0;
}
int bccode(void) {pcc_note_frame((unsigned)autooff/8);return 0;}
int efcode(void) {
    deflab(retlab);printf(" leave\n ret\n.set .F%d,%u\n.size %s,.-%s\n",ftnno,(frame_size+15)&~15U,stab[curftn].sname,stab[curftn].sname);return 0;
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
int fincode(double value,int bits) {
    if(bits==32) {float f=(float)value;uint32_t v;memcpy(&v,&f,4);printf(".long %u\n",v);}
    else if(bits==64) {uint64_t v;memcpy(&v,&value,8);printf(".quad %lu\n",(unsigned long)v);}
    else cerror("invalid floating initializer width");
    inoff+=bits;return 0;
}
int ecode(NODE *tree) {
    if(nerrors)return 0;
    struct mip_node *nodes=calloc(TREESZ,sizeof(*nodes));if(!nodes)cerror("no memory for target IR");
    struct mip_arena arena={nodes,0,TREESZ};uint32_t root=pcc_lower(tree,&arena);
    if(pcc_output_tree(&arena,root,0))cerror("expression pass output failed");
    free(nodes);return 0;
}
int cinit(NODE *tree,int bits) {
    if(nerrors)return 0;
    if(initial_bits)cerror("unaligned scalar initializer");
    struct mip_node *nodes=calloc(TREESZ,sizeof(*nodes));if(!nodes)cerror("no memory for initializer IR");
    struct mip_arena arena={nodes,0,TREESZ};uint32_t root=pcc_lower(tree,&arena);
    if(pcc_output_tree(&arena,root,(unsigned)bits))cerror("initializer pass output failed");
    inoff+=bits;free(nodes);return 0;
}
int main(int argc,char **argv) {
    unsigned emit=0,read=0;int source=1;
    if(argc>1 && !strcmp(argv[1],"--emit-pass")) {emit=1;source++;}
    else if(argc>1 && !strcmp(argv[1],"--read-pass")) {read=1;source++;}
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

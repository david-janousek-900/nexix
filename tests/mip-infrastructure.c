/* Private consumer of native mip services; the same checks run in Nexix. */
#include "mip.h"
#include "templates.h"
#include "diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef MIP_DEEP_COUNT
#define MIP_DEEP_COUNT 1024
#endif
#define CHECK(x) do {if(!(x)){printf("FAIL: mip line %d\n",__LINE__);return 1;}}while(0)
struct visits {uint32_t order[16];unsigned count;};
static int visit(const struct mip_node *n,uint32_t i,void *p) {(void)n;struct visits *v=p;v->order[v->count++]=i;return 0;}
static int stop(const struct mip_node *n,uint32_t i,void *p) {(void)n;(void)i;(void)p;return -7;}
static int increment(const struct mip_node *n,uint32_t i,void *p) {(void)n;(void)i;(*(unsigned *)p)++;return 0;}
struct expansion {FILE *file;unsigned fields;};
static int emit(void *ctx,char escape,unsigned modifier,const struct mip_node *n) {
    struct expansion *e=ctx;
    if(!escape)return fputc((int)modifier,e->file)==EOF;
    if(escape=='C')return fprintf(e->file,"%ld",(long)(int64_t)n->value)<0;
    if(escape=='A')return fprintf(e->file,"%%r%u",n->reg)<0;
    if(escape=='L')return fprintf(e->file,"%u",n->label)<0;
    if(escape=='S' || escape=='H' || escape=='M' || escape=='N' || escape=='T' || escape=='Z' || escape=='O' || escape=='B' || escape=='I' || escape=='U') {e->fields++;return 0;}
    return -1;
}
static int run(FILE *assembly,FILE *pass,FILE *scratch) {
    struct mip_node nodes[16],copy[16],decoded[16];struct mip_arena a,b,c;uint32_t id,root;
    mip_arena_init(&a,nodes,16);mip_arena_init(&b,copy,16);mip_arena_init(&c,decoded,16);
    struct mip_node n={.op=ICON,.type=LONG,.left=MIP_NO_NODE,.right=MIP_NO_NODE,.value=19,.source_line=42};
    CHECK(!mip_node_add(&a,&n,&id));n.value=23;CHECK(!mip_node_add(&a,&n,&id));
    n.op=PLUS;n.left=0;n.right=1;CHECK(!mip_node_add(&a,&n,&root));
    struct visits v={0};CHECK(!mip_walk(&a,root,visit,&v));CHECK(v.count==3 && v.order[0]==0 && v.order[1]==1 && v.order[2]==2);
    CHECK(mip_walk(&a,root,stop,NULL)==-7);
    CHECK(!mip_copy(&a,root,&b,&id) && id==2 && b.count==3 && b.nodes[2].left==0);
    struct mip_arena short_arena={copy,0,2};id=99;
    CHECK(mip_copy(&a,root,&short_arena,&id)==-1 && !short_arena.count && id==99);
    CHECK(mip_copy(&a,root,&a,&id)==-1);
    unsigned char *wire=NULL;size_t length=0;
    CHECK(!mip_encode(&a,root,&wire,&length) && length==280);
    CHECK(!mip_decode(wire,length,&c,&id) && id==root && c.count==3);
    CHECK(c.nodes[0].value==19 && c.nodes[1].value==23 && c.nodes[0].source_line==42);
    /* Version 2 carries binary80 bits independently of the binary64 slot. */
    b.nodes[0].type=LDOUBLE;b.nodes[0].extended[0]=1;b.nodes[0].extended[7]=0x80;
    b.nodes[0].extended[8]=0xff;b.nodes[0].extended[9]=0x3f;
    unsigned char *extended=NULL;size_t extended_size=0;
    CHECK(!mip_encode(&b,root,&extended,&extended_size));
    CHECK(!memcmp(extended+60,b.nodes[0].extended,10));
    mip_arena_init(&c,decoded,16);
    CHECK(!mip_decode(extended,extended_size,&c,&id) && c.nodes[0].type==LDOUBLE && !memcmp(c.nodes[0].extended,b.nodes[0].extended,10));
    free(extended);mip_arena_init(&c,decoded,16);CHECK(!mip_decode(wire,length,&c,&id));
    unsigned char *again=NULL;size_t other=0;CHECK(!mip_encode(&c,id,&again,&other) && other==length && !memcmp(wire,again,length));free(again);
    for(size_t i=0;i<length;i++) {
        struct mip_arena truncated={decoded,0,16};id=123;
        CHECK(mip_decode(wire,i,&truncated,&id)==-1 && !truncated.count && id==123);
    }
    unsigned char saved=wire[8];wire[8]=255;mip_arena_init(&c,decoded,16);CHECK(mip_decode(wire,length,&c,&id)==-1);wire[8]=saved;
    saved=wire[16+12];wire[16+12]=0;CHECK(mip_decode(wire,length,&c,&id)==-1);wire[16+12]=saved;
    CHECK(!mip_decode(wire,length,&c,&id));
    CHECK(fwrite(wire,1,length,pass)==length);
    uint32_t random=192;
    for(unsigned test=0;test<1000;test++) {
        random=random*1664525U+1013904223U;size_t at=random%length;unsigned char old=wire[at];
        random=random*1664525U+1013904223U;wire[at]=(unsigned char)(random>>24);
        mip_arena_init(&c,decoded,16);id=123;
        if(!mip_decode(wire,length,&c,&id)) {
            unsigned char *roundtrip=NULL;size_t size=0;CHECK(!mip_encode(&c,id,&roundtrip,&size));free(roundtrip);
        } else CHECK(!c.count && id==123);
        wire[at]=old;
    }
    free(wire);
    struct mip_register regs[]={ {"r0",SAREG|STAREG,3},{"r0_alias",SAREG|STAREG,3},
        {"r2",SAREG|STAREG,4},{"b0",SBREG|STBREG,8}};
    struct mip_resources r;CHECK(!mip_resources_init(&r,regs,4,64));
    CHECK(!mip_register_hold(&r,0));CHECK(mip_register_hold(&r,1)==-1);CHECK(!mip_register_hold(&r,0));
    CHECK(!mip_register_release(&r,0) && !mip_register_release(&r,0) && mip_register_release(&r,0)==-1);
    struct mip_allocation allocation;CHECK(!mip_allocate(&r,2*NAREG|NBREG|NTEMP,24,16,&allocation));
    CHECK(allocation.count==3 && allocation.regs[0]==0 && allocation.regs[1]==2 && allocation.regs[2]==3 && allocation.temporary==0 && allocation.size==24);
    struct mip_resources previous=r;CHECK(mip_allocate(&r,NAREG,8,8,&allocation)==-1 && !memcmp(&r,&previous,sizeof(r)));
    CHECK(!mip_register_release(&r,0) && !mip_register_release(&r,2) && !mip_register_release(&r,3));
    CHECK(!mip_allocate(&r,NTEMP,8,16,&allocation) && allocation.temporary==32 && r.temporary==40);
    previous=r;CHECK(mip_allocate(&r,NTEMP,64,16,&allocation)==-1 && !memcmp(&r,&previous,sizeof(r)));
    CHECK(mip_allocate(&r,NTEMP,8,3,&allocation)==-1 && !memcmp(&r,&previous,sizeof(r)));
    CHECK(mip_operator_match(ASG PLUS,ASG OPSIMP) && !mip_operator_match(ASG PLUS,OPSIMP));
    CHECK(mip_operator_match(PLUS,OPANY) && !mip_operator_match(ASSIGN,OPANY));
    CHECK(!mip_operator_match(DSIZE,OPANY) && mip_operator_match(ICON,OPLTYPE));
    mip_type ptr,array;CHECK(!mip_type_add(INT,ARY,&array) && !mip_type_add(array,PTR,&ptr));
    CHECK(mip_type_match(ptr,TPTRTO|TINT) && mip_type_match(ptr,TPOINT) && !mip_type_match(ptr,TLONG));
    CHECK(mip_type_match(ULONG,TULONG) && !mip_type_match(ULONG,TINT));
    CHECK(mip_shape_match(&a,0,SCON,&r));
    struct mip_template table[]={
        {PLUS,INAREG,SCON,TLONG,SCON,TLONG,3*NAREG,0,"bad"},
        {OPSIMP,INAREG,SCON,TLONG,SCON,TLONG,NAREG,RESC1,".text\n.globl ir_value\n.type ir_value,@function\nir_value:\n movq $CL,%rax\n addq $CR,%rax\n ret\n"},
        {OPANY,FORREW,SANY,TANY,SANY,TANY,REWRITE,RLEFT,NULL}
    };
    CHECK(!mip_resources_init(&r,regs,4,64));size_t selected=99;
    CHECK(mip_match(&a,root,INAREG,table,3,&r,&selected,&allocation)==1 && selected==1 && allocation.count==1);
    CHECK(r.busy[0]==1 && !r.busy[2]);
    struct expansion e={assembly,0};CHECK(!mip_expand(&a,root,INAREG,table[selected].text,NULL,emit,&e));
    previous=r;selected=99;CHECK(mip_match(&a,root,FORCC,table,3,&r,&selected,&allocation)==0 && selected==99 && !memcmp(&r,&previous,sizeof(r)));
    CHECK(mip_match(&a,root,FORREW,table,3,&r,&selected,&allocation)==1 && selected==2 && !memcmp(&r,&previous,sizeof(r)));
    struct expansion fields={scratch,0};
    CHECK(!mip_expand(&a,root,FOREFF,"Fomitted\nZxOxSHMNT",NULL,emit,&fields) && fields.fields==7);
    CHECK(mip_expand(&a,root,FOREFF,"A",NULL,emit,&fields)==-1);
    CHECK(mip_expand(&a,root,FOREFF,"A9",NULL,emit,&fields)==-1);
    struct mip_diagnostics d;
    mip_diagnostics_init(&d,scratch,"large.c",2);
    CHECK(!mip_diagnostic(&d,12,1,"pointer %s %ld","argument",0x100000001L));
    CHECK(!mip_diagnostic(&d,13,0,"first %u",7U));
    CHECK(mip_diagnostic(&d,14,0,"second")==-1 && d.errors==2 && d.warnings==1 && d.fatal);
    CHECK(mip_diagnostic(&d,15,0,"suppressed")==-1);CHECK(!fflush(scratch) && !fseek(scratch,0,SEEK_SET));
    char diagnostics[256]={0};size_t got=fread(diagnostics,1,sizeof(diagnostics)-1,scratch);
    CHECK(got && !strcmp(diagnostics,"large.c:12: warning: pointer argument 4294967297\nlarge.c:13: error: first 7\nlarge.c:14: error: second\n"));
    /* Deep trees use heap worklists instead of the guest's small user stack. */
    struct mip_node *deep=calloc(MIP_DEEP_COUNT,sizeof(*deep));CHECK(deep);struct mip_arena da={deep,0,MIP_DEEP_COUNT};
    n=(struct mip_node){.op=ICON,.type=LONG,.left=MIP_NO_NODE,.right=MIP_NO_NODE};CHECK(!mip_node_add(&da,&n,&id));
    for(unsigned i=1;i<MIP_DEEP_COUNT;i++) {n.op=UNARY MINUS;n.left=i-1;CHECK(!mip_node_add(&da,&n,&id));}
    unsigned visited=0;CHECK(!mip_walk(&da,MIP_DEEP_COUNT-1,increment,&visited) && visited==MIP_DEEP_COUNT);free(deep);
    puts("PASS: mip templates, resources, traversal, diagnostics and pass interface");return 0;
}
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    FILE *assembly=fopen(argv[1],"w"),*pass=fopen(argv[2],"w"),*scratch=fopen(argv[3],"w+");
    if(!assembly || !pass || !scratch)return 1;
    int result=run(assembly,pass,scratch);if(fclose(assembly) || fclose(pass) || fclose(scratch))return 1;return result;
}

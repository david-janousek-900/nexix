/* Separate analysis consumer: no PCC headers, globals or target dependencies. */
#include "pass.h"
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: mip pass line %d\n",__LINE__);return 1;}}while(0)
struct analysis {unsigned nodes,expressions,initializers,names,texts;int fail;};
static int visit(const struct mip_node *n,uint32_t id,void *context) {
    (void)id;struct analysis *s=context;s->nodes++;s->names+=n->name[0]!=0;return 0;
}
static int text(void *context,const unsigned char *p,size_t n) {
    struct analysis *s=context;s->texts++;
    for(size_t i=0;i<n;i++)if(!p[i])return -1;
    return s->fail?-1:0;
}
static int tree(void *context,const struct mip_arena *a,uint32_t root,unsigned bits) {
    struct analysis *s=context;if(s->fail)return -1;
    s->expressions+=!bits;s->initializers+=bits!=0;
    /* Analysis clients share traversal/types without linking machine emission. */
    return mip_walk(a,root,visit,s);
}
static const struct mip_pass_consumer consumer={text,tree};
static int read_bytes(FILE *scratch,const unsigned char *p,size_t n,struct analysis *s) {
    if(fseek(scratch,0,SEEK_SET) || fwrite(p,1,n,scratch)!=n || fflush(scratch))return -2;
    /* The caller truncates logically by testing every prefix in a fresh FILE. */
    if(fseek(scratch,0,SEEK_SET))return -2;
    return mip_pass_read(scratch,&consumer,s);
}
static int self(const char *out,const char *scratch_path) {
    struct mip_node nodes[8];struct mip_arena a={nodes,0,8};uint32_t id,root;
    struct mip_node n={.op=ICON,.type=ULONG,.left=MIP_NO_NODE,.right=MIP_NO_NODE,.value=UINT64_MAX,.source_line=123};
    CHECK(!mip_node_add(&a,&n,&id));n.op=NAME;n.type=LONG;n.value=0;n.offset=INT64_MIN;
    memset(n.name,'q',MIP_NAME_MAX);n.name[MIP_NAME_MAX]=0;CHECK(!mip_node_add(&a,&n,&id));
    n=(struct mip_node){.op=PLUS,.type=ULONG,.left=0,.right=1};CHECK(!mip_node_add(&a,&n,&root));
    FILE *file=fopen(out,"w");CHECK(file);struct mip_pass_writer w;
    CHECK(!mip_pass_begin(&w,file));CHECK(!mip_pass_text(&w,"alternate\n",10));
    CHECK(!mip_pass_tree(&w,&a,root,0));CHECK(!mip_pass_tree(&w,&a,0,64));
    n=(struct mip_node){.op=FCON,.type=DOUBLE,.left=MIP_NO_NODE,.right=MIP_NO_NODE,.floating=1.25};
    CHECK(!mip_node_add(&a,&n,&id));CHECK(!mip_pass_tree(&w,&a,id,0));
    n=(struct mip_node){.op=STARG,.type=STRTY,.left=1,.right=MIP_NO_NODE,.aggregate_size=16,.aggregate_alignment=8};
    CHECK(!mip_node_add(&a,&n,&id));CHECK(!mip_pass_tree(&w,&a,id,0));
    CHECK(!mip_pass_text(&w,"",0));CHECK(!mip_pass_end(&w));size_t length=w.bytes;
    CHECK(mip_pass_end(&w)==-1 && mip_pass_text(&w,"x",1)==-1 && w.bytes==length);CHECK(!fclose(file));
    file=fopen(out,"r");CHECK(file);struct analysis s={0};CHECK(!mip_pass_read(file,&consumer,&s));CHECK(!fclose(file));
    CHECK(s.nodes==7 && s.names==2 && s.expressions==3 && s.initializers==1 && s.texts==2);
    unsigned char *bytes=malloc(length+1);CHECK(bytes);file=fopen(out,"r");CHECK(file);
    CHECK(fread(bytes,1,length,file)==length && !fclose(file));bytes[length]=0;
    /* Truncation at every position, including exact record boundaries, must
     * fail: physical EOF is not a substitute for the explicit END record. */
    for(size_t i=0;i<length;i++) {
        FILE *f=fopen(scratch_path,"w+");CHECK(f);s=(struct analysis){0};
        CHECK(read_bytes(f,bytes,i,&s)==-1);CHECK(!fclose(f));
    }
    const size_t positions[]={0,6,8,12,16,20,24,38,42,46};
    for(unsigned i=0;i<sizeof(positions)/sizeof(*positions);i++) {
        FILE *f=fopen(scratch_path,"w+");CHECK(f);size_t at=positions[i];unsigned char old=bytes[at];bytes[at]=255;s=(struct analysis){0};
        CHECK(read_bytes(f,bytes,length,&s)==-1);bytes[at]=old;CHECK(!fclose(f));
    }
    file=fopen(scratch_path,"w+");CHECK(file);s=(struct analysis){0};
    CHECK(read_bytes(file,bytes,length+1,&s)==-1);CHECK(!fclose(file));
    file=fopen(out,"r");CHECK(file);s=(struct analysis){.fail=1};
    CHECK(mip_pass_read(file,&consumer,&s)==-1 && s.texts==1 && !s.nodes);CHECK(!fclose(file));
    file=fopen(scratch_path,"w+");CHECK(file);CHECK(!mip_pass_begin(&w,file));
    CHECK(mip_pass_text(&w,"bad\0text",8)==-1 && mip_pass_end(&w)==-1);CHECK(!fclose(file));
    file=fopen(scratch_path,"w+");CHECK(file);CHECK(!mip_pass_begin(&w,file));
    CHECK(mip_pass_text(&w,"x",MIP_PASS_TEXT_LIMIT+1UL)==-1 && mip_pass_end(&w)==-1);CHECK(!fclose(file));
    file=fopen(scratch_path,"w+");CHECK(file);CHECK(!mip_pass_begin(&w,file));
    CHECK(mip_pass_tree(&w,&a,0,7)==-1 && mip_pass_end(&w)==-1);CHECK(!fclose(file));
    file=fopen(scratch_path,"w+");CHECK(file);CHECK(!mip_pass_begin(&w,file));w.bytes=MIP_PASS_LIMIT-6;
    CHECK(mip_pass_text(&w,"x",1)==-1 && mip_pass_end(&w)==-1);CHECK(!fclose(file));
    file=fopen(out,"r");CHECK(file);CHECK(mip_pass_begin(&w,file) || mip_pass_end(&w));fclose(file);
    free(bytes);puts("PASS: shared mip stream validation and independent analysis consumer");return 0;
}
int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--check")) {
        FILE *file=fopen(argv[2],"r");if(!file)return 1;struct analysis s={0};
        int status=mip_pass_read(file,&consumer,&s);if(fclose(file))status=-1;if(status)return 1;
        printf("nodes=%u expressions=%u initializers=%u names=%u\n",s.nodes,s.expressions,s.initializers,s.names);return 0;
    }
    if(argc!=3)return 2;
    return self(argv[1],argv[2]);
}

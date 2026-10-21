#include "mip.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL: line %d\n",__LINE__); return 1; } } while (0)
int main(void) {
    struct mip_layout l;
    mip_type t=ULONG, u=0;
    uint64_t aligned;
    static const unsigned expected[16]={0,0,1,2,4,8,4,8,0,0,4,0,1,2,4,8};
    for(unsigned i=0;i<16;i++) {
        int r=mip_scalar_layout(i,&l);
        CHECK(expected[i] ? (!r && l.size==expected[i] && l.alignment==expected[i]) : r==-1);
        CHECK(!mip_type_add(i,PTR,&u));
        CHECK(!mip_scalar_layout(u,&l) && l.size==8 && l.alignment==8);
    }
    for(unsigned i=0;i<MIP_MAX_MODIFIERS;i++) CHECK(!mip_type_add(t,i%2?ARY:PTR,&t));
    u=123; CHECK(mip_type_add(t,PTR,&u)==-1 && u==123);
    for(unsigned i=0;i<MIP_MAX_MODIFIERS;i++) CHECK(!mip_type_remove(t,&t));
    CHECK(t==ULONG && mip_type_remove(t,&u)==-1);
    CHECK(!mip_type_valid(INT | ((mip_type)PTR<<2)));
    CHECK(mip_type_add(INT,3,&u)==-1);
    CHECK(!mip_type_add(INT,FTN,&u) && mip_scalar_layout(u,&l)==-1);
    CHECK(!mip_type_add(u,PTR,&u) && !mip_scalar_layout(u,&l) && l.size==8);
    CHECK(!mip_align(0x100000001UL,16,&aligned) && aligned==0x100000010UL);
    aligned=123; CHECK(mip_align(UINT64_MAX,8,&aligned)==-1 && aligned==123);
    CHECK(mip_align(1,0,&aligned)==-1 && mip_align(1,3,&aligned)==-1);
    CHECK(!mip_align(UINT64_MAX,1,&aligned) && aligned==UINT64_MAX);
    CHECK((mip_op_flags(ASG PLUS)&(TYFLG|ASGFLG))==(BITYPE|ASGFLG));
    CHECK(mip_op_flags(UNARY STCALL)&CALLFLG);
    CHECK(!mip_op_flags(DSIZE) && !mip_op_name(DSIZE));
    CHECK(!strcmp(mip_op_name(PLUS),"+"));
    struct mip_node nodes[4], n={0}; struct mip_arena a; uint32_t id=99;
    mip_arena_init(&a,nodes,4);
    n.op=ICON; n.type=ULONG; n.left=n.right=MIP_NO_NODE; n.value=UINT64_MAX;
    CHECK(!mip_node_add(&a,&n,&id) && id==0 && nodes[0].value==UINT64_MAX);
    n.op=NAME; strcpy(n.name,"identifier_beyond_eight_characters"); n.offset=-0x100000001L;
    CHECK(!mip_node_add(&a,&n,&id) && id==1 && nodes[1].offset==-0x100000001L);
    CHECK(!strcmp(nodes[1].name,n.name));
    n.op=PLUS; n.left=0; n.right=1;
    CHECK(!mip_node_add(&a,&n,&id) && id==2);
    id=77; n.right=3; CHECK(mip_node_add(&a,&n,&id)==-1 && id==77 && a.count==3);
    n.op=STASG; n.right=1; n.aggregate_size=17; n.aggregate_alignment=8;
    CHECK(mip_node_add(&a,&n,&id)==-1 && a.count==3);
    n.aggregate_size=24;
    CHECK(!mip_node_add(&a,&n,&id) && id==3);
    CHECK(mip_node_add(&a,&n,&id)==-1 && a.count==4);
    mip_arena_init(&a,nodes,4);
    n.op=ICON; n.left=n.right=MIP_NO_NODE;
    memset(n.name,'x',sizeof(n.name)); CHECK(mip_node_add(&a,&n,&id)==-1 && !a.count);
    n.name[sizeof(n.name)-1]=0; CHECK(!mip_node_add(&a,&n,&id));
    n.op=UNARY MUL; n.left=0; CHECK(!mip_node_add(&a,&n,&id));
    n.right=0; CHECK(mip_node_add(&a,&n,&id)==-1);
    mip_arena_init(&a,0,4); CHECK(mip_node_add(&a,&n,&id)==-1);
    puts("PASS: native mip types, operators and bounded IR");
    return 0;
}

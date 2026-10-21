/* Checked generic resource allocation; physical register tables belong to targets. */
#include "templates.h"
#include <string.h>
#include <limits.h>
int mip_resources_init(struct mip_resources *r,const struct mip_register *regs,unsigned count,uint64_t limit) {
    if(!r || count>MIP_REG_LIMIT || (count&&!regs) || limit>INT64_MAX)return -1;
    for(unsigned i=0;i<count;i++) {
        uint64_t self=UINT64_C(1)<<i;
        if(!regs[i].name || !*regs[i].name || !(regs[i].aliases&self) || (count<64 && (regs[i].aliases>>count)))return -1;
        unsigned base=regs[i].shapes&(SAREG|SBREG);
        if(base!=SAREG && base!=SBREG)return -1;
        if((regs[i].shapes&STAREG) && base!=SAREG)return -1;
        if((regs[i].shapes&STBREG) && base!=SBREG)return -1;
        for(unsigned j=0;j<count;j++)if(((regs[i].aliases>>j)&1)!=((regs[j].aliases>>i)&1))return -1;
    }
    memset(r,0,sizeof(*r));r->registers=regs;r->count=count;r->temporary_limit=limit;return 0;
}
int mip_register_hold(struct mip_resources *r,unsigned reg) {
    if(!r || r->count>MIP_REG_LIMIT || reg>=r->count || r->busy[reg]==UINT_MAX)return -1;
    for(unsigned i=0;i<r->count;i++)if(i!=reg && r->busy[i] && ((r->registers[reg].aliases>>i)&1))return -1;
    r->busy[reg]++;return 0;
}
int mip_register_release(struct mip_resources *r,unsigned reg) {
    if(!r || r->count>MIP_REG_LIMIT || reg>=r->count || !r->busy[reg])return -1;
    r->busy[reg]--;return 0;
}
int mip_allocate(struct mip_resources *r,unsigned needs,unsigned size,unsigned alignment,struct mip_allocation *result) {
    if(!r || !result || r->count>MIP_REG_LIMIT || (r->count&&!r->registers) ||
       (needs&~(NAMASK|NBMASK|NTMASK)))return -1;
    struct mip_resources copy=*r;struct mip_allocation out={0};
    unsigned a=needs&NACOUNT,b=(needs&NBCOUNT)/NBREG;
    if(a+b>3)return -1;
    for(unsigned c=0;c<a+b;c++) {
        unsigned shape=c<a?STAREG:STBREG;int chosen=-1;
        for(unsigned i=0;i<r->count;i++) {
            if(!(r->registers[i].shapes&shape))continue;
            int free=1;
            for(unsigned j=0;j<r->count;j++)if(copy.busy[j] && ((r->registers[i].aliases>>j)&1)) {free=0;break;}
            if(free) {chosen=(int)i;break;}
        }
        if(chosen<0)return -1;
        copy.busy[chosen]++;out.regs[out.count++]=chosen;
    }
    if(needs&NTMASK) {
        uint64_t n=(needs&NTMASK)/NTEMP*8,offset;
        if(n<size)n=size;
        if(mip_align(copy.temporary,alignment,&offset) || offset>copy.temporary_limit || n>copy.temporary_limit-offset)return -1;
        out.temporary=offset;out.size=n;copy.temporary=offset+n;
    }
    *r=copy;*result=out;return 0;
}

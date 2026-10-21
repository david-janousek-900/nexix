/* Native checked adaptation of mip/match.c table matching and expansion. */
#include "templates.h"
#include <string.h>
int mip_operator_match(unsigned op,unsigned pattern) {
    static const unsigned masks[]={SIMPFLG,SIMPFLG|ASGFLG,COMMFLG,COMMFLG|ASGFLG,
        MULFLG,MULFLG|ASGFLG,DIVFLG,DIVFLG|ASGFLG,UTYPE,TYFLG,LTYPE,TYFLG,0,
        ASGOPFLG|ASGFLG,LOGFLG,TYFLG,FLOFLG,FLOFLG|ASGFLG,SHFFLG,SHFFLG|ASGFLG,SPFLG,TYFLG};
    unsigned flags=mip_op_flags(op);if(!flags)return 0;
    if(pattern<OPSIMP)return op==pattern;
    unsigned i=pattern-OPSIMP;if(i>=sizeof(masks)/sizeof(*masks))return 0;
    if(masks[i]&SPFLG)return op==NAME || op==ICON || op==OREG || op==UNARY MUL;
    return (flags&(masks[i]|ASGFLG))==masks[i];
}
int mip_type_match(mip_type t,unsigned mask) {
    static const unsigned types[21]={0,0,TCHAR,TSHORT,TINT,TLONG,TFLOAT,TDOUBLE,TSTRUCT,TSTRUCT,TINT,0,TUCHAR,TUSHORT,TUNSIGNED,TULONG,TLLONG,TULLONG,TLDOUBLE,0,TUCHAR};
    if(!mip_type_valid(t))return 0;
    if(mask&TANY)return 1;
    if((t&TMASK)==PTR && (mask&TPTRTO)) {
        if(mip_type_remove(t,&t))return 0;
        while((t&TMASK)==ARY)if(mip_type_remove(t,&t))return 0;
        return mip_type_match(t,mask&~TPTRTO);
    }
    if(t&TMASK)return (mask&TPOINT)!=0;
    return !(mask&TPTRTO) && (mask&types[t])!=0;
}
static const struct mip_node *operand(const struct mip_arena *a,uint32_t root,int side) {
    const struct mip_node *n=a->nodes+root;
    uint32_t id=side=='L'?n->left:n->right;
    return id==MIP_NO_NODE?n:(id<a->count?a->nodes+id:NULL);
}
int mip_shape_match(const struct mip_arena *a,uint32_t i,unsigned shape,const struct mip_resources *r) {
    if(!a || !a->nodes || i>=a->count)return 0;
    const struct mip_node *n=a->nodes+i;
    if(shape&SPECIAL) {
        if(n->op!=ICON || n->name[0])return 0;
        return shape==SZERO?n->value==0:shape==SONE?n->value==1:shape==SMONE?n->value==UINT64_MAX:0;
    }
    if(shape&SANY)return 1;
    if((shape&SWADD) && (n->op==NAME || n->op==OREG) && ((uint64_t)n->offset&7))return 0;
    switch(n->op) {
    case NAME:return (shape&SNAME)!=0;
    case ICON:return (shape&SCON)!=0;
    case OREG:return (shape&(SOREG|INTEMP))!=0;
    case CCODES:return (shape&SCC)!=0;
    case FLD:
        if(!(shape&SFLD) || n->left>=a->count || !n->field_size || n->field_size>64 || n->field_shift>64-n->field_size)return 0;
        return a->nodes[n->left].op==NAME || a->nodes[n->left].op==OREG || a->nodes[n->left].op==UNARY MUL;
    case REG: {
        if(!r || r->count>MIP_REG_LIMIT || n->reg>=r->count)return 0;
        unsigned flags=r->registers[n->reg].shapes;
        if(r->busy[n->reg]>1)flags&=~(STAREG|STBREG);
        return (flags&shape)!=0;
    }
    case UNARY MUL:
        if(n->left>=a->count)return 0;
        return a->nodes[n->left].op==REG?(shape&STARREG)!=0:
               (a->nodes[n->left].op==NAME || a->nodes[n->left].op==ICON)?(shape&STARNM)!=0:0;
    default:return 0;
    }
}
int mip_match(const struct mip_arena *a,uint32_t root,unsigned cookie,const struct mip_template *table,size_t count,
              struct mip_resources *resources,size_t *selected,struct mip_allocation *allocation) {
    if(!a || !a->nodes || root>=a->count || !table || !resources || !selected || !allocation)return -1;
    const struct mip_node *p=a->nodes+root,*left=operand(a,root,'L'),*right=operand(a,root,'R');
    if(!left || !right)return -1;
    struct mip_arena prefix={a->nodes,root,a->count};
    if(mip_node_valid(&prefix,p))return -1;
    prefix.count=(uint32_t)(left-a->nodes);if(mip_node_valid(&prefix,left))return -1;
    prefix.count=(uint32_t)(right-a->nodes);if(mip_node_valid(&prefix,right))return -1;
    for(size_t i=0;i<count;i++) {
        const struct mip_template *t=table+i;
        if(!mip_operator_match(p->op,t->op) || !(cookie&t->visit) ||
           !mip_shape_match(a,(uint32_t)(left-a->nodes),t->lshape,resources) || !mip_type_match(left->type,t->ltype) ||
           !mip_shape_match(a,(uint32_t)(right-a->nodes),t->rshape,resources) || !mip_type_match(right->type,t->rtype))continue;
        struct mip_resources copy=*resources;struct mip_allocation result={0};
        if(!(t->needs&REWRITE)) {
            if(left->op==REG && left->reg<copy.count && copy.busy[left->reg]==1 &&
               (((t->needs&NASL) && (copy.registers[left->reg].shapes&SAREG)) ||
                ((t->needs&NBSL) && (copy.registers[left->reg].shapes&SBREG))))
                if(mip_register_release(&copy,left->reg))return -1;
            if(right!=left && right->op==REG && right->reg<copy.count && copy.busy[right->reg]==1 &&
               (((t->needs&NASR) && (copy.registers[right->reg].shapes&SAREG)) ||
                ((t->needs&NBSR) && (copy.registers[right->reg].shapes&SBREG))))
                if(mip_register_release(&copy,right->reg))return -1;
            struct mip_layout layout;unsigned size=0,alignment=8;
            if(p->aggregate_alignment) {if(p->aggregate_size>UINT32_MAX)return -1;size=(unsigned)p->aggregate_size;alignment=p->aggregate_alignment;}
            else if(!mip_scalar_layout(p->type,&layout)) {size=(unsigned)layout.size;alignment=layout.alignment;}
            if(mip_allocate(&copy,t->needs,size,alignment,&result))continue;
        }
        *resources=copy;*selected=i;*allocation=result;return 1;
    }
    return 0;
}
int mip_expand(const struct mip_arena *a,uint32_t root,unsigned cookie,const char *text,const struct mip_node results[3],mip_expand_callback emit,void *context) {
    if(!a || !a->nodes || root>=a->count || !text || !emit)return -1;
    const struct mip_node *p=a->nodes+root;
    for(size_t i=0;text[i];i++) {
        char escape=text[i];unsigned modifier=0;const struct mip_node *n=p;
        if(escape=='F') {
            if(cookie&FOREFF)while(text[i] && text[i]!='\n')i++;
            if(!text[i])break;
            continue;
        }
        if(escape=='T' || escape=='S' || escape=='H' || escape=='M' || escape=='N' || escape=='L') {
            if(emit(context,escape,0,p))return -1;
            continue;
        }
        if(escape=='A' || escape=='C' || escape=='I' || escape=='U' || escape=='B') {
            modifier=(unsigned char)text[++i];if(!modifier)return -1;
            if(modifier=='L' || modifier=='R')n=operand(a,root,(int)modifier);
            else if(modifier>='1' && modifier<='3' && results)n=results+modifier-'1';
            else return -1;
            if(!n)return -1;
        } else if(escape=='Z' || escape=='O') {modifier=(unsigned char)text[++i];if(!modifier)return -1;}
        else {modifier=(unsigned char)escape;escape=0;}
        if(emit(context,escape,modifier,n))return -1;
    }
    return 0;
}

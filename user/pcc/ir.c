/* Bridge retained mutable PCC trees into the checked native mip DAG. */
#include "mfile1"
struct bridge_context {struct mip_arena *arena;uint32_t map[TREESZ];};
static struct bridge_context *current_bridge;
static unsigned slot(NODE *p) {
    uintptr_t base=(uintptr_t)node,address=(uintptr_t)p;
    if(address<base || address-base>=sizeof(NODE)*TREESZ || (address-base)%sizeof(NODE))cerror("invalid parse tree address");
    return (unsigned)((address-base)/sizeof(NODE));
}
static uint32_t child(NODE *p) {
    uint32_t i=current_bridge->map[slot(p)];
    if(i==MIP_NO_NODE)cerror("invalid parse tree traversal");return i;
}
static int lower_node(NODE *p) {
    unsigned index=slot(p);if(current_bridge->map[index]!=MIP_NO_NODE)return 0;
    struct mip_node n={0};uint32_t id;unsigned kind=(unsigned)optype(p->op);
    n.op=(unsigned)p->op;n.type=p->type;n.left=n.right=MIP_NO_NODE;n.value=(uint64_t)p->lval;n.floating=p->dval;n.source_line=(unsigned)lineno;
    if(kind!=LTYPE)n.left=child(p->left);
    if(kind==BITYPE)n.right=child(p->right);
    if(p->op==REG)n.reg=(unsigned)p->rval;
    if(p->op==NAME || p->op==ICON) {
        if(p->rval!=NONAME && p->rval>=0) {
            if(p->rval>=SYMTSZ)cerror("invalid name index");
            struct symtab *s=stab+p->rval;n.reg=(unsigned)s->sclass;n.label=s->slevel;
            if(s->sclass==AUTO || s->sclass==PARAM)n.offset=s->offset/8;
            if(s->sclass==STATIC && s->slevel>1)snprintf(n.name,sizeof(n.name),".L%d",s->offset);
            else snprintf(n.name,sizeof(n.name),"%s",s->sname);
        } else if(p->rval<0)snprintf(n.name,sizeof(n.name),".L%d",-p->rval);
    }
    if(p->op==FLD) {n.field_size=UPKFSZ(p->rval);n.field_shift=UPKFOFF(p->rval);}
    if(p->op==STASG || p->op==STARG || p->op==STCALL || p->op==UNARY STCALL) {
        n.aggregate_size=(uint64_t)tsize(STRTY,p->left->cdim,p->left->csiz)/8;n.aggregate_alignment=(unsigned)talign(STRTY,p->left->csiz)/8;
    }
    if(mip_node_add(current_bridge->arena,&n,&id))cerror("cannot lower front-end node %d",p->op);
    current_bridge->map[index]=id;return 0;
}
uint32_t pcc_lower(NODE *p,struct mip_arena *arena) {
    struct bridge_context *ctx=malloc(sizeof(*ctx));if(!ctx)cerror("no memory for parse tree bridge");
    ctx->arena=arena;unsigned i;for(i=0;i<TREESZ;i++)ctx->map[i]=MIP_NO_NODE;
    current_bridge=ctx;walkf(p,lower_node);uint32_t root=ctx->map[slot(p)];current_bridge=NULL;free(ctx);return root;
}

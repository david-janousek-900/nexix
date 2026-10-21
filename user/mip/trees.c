/* Bounded native equivalents of common's walkf/tcopy and tree pass interface. */
#include "mip.h"
#include <stdlib.h>
#include <string.h>
static int valid(const struct mip_arena *a,uint32_t root) {
    if(!a || !a->nodes || !a->count || a->count>a->capacity || a->count>MIP_NODE_LIMIT || root>=a->count)return -1;
    for(uint32_t i=0;i<a->count;i++) {
        struct mip_arena prefix={a->nodes,i,a->count};
        if(mip_node_valid(&prefix,a->nodes+i))return -1;
    }
    return 0;
}
struct frame {uint32_t node;unsigned phase;};
int mip_walk(const struct mip_arena *a,uint32_t root,mip_visitor visit,void *ctx) {
    if(!visit || valid(a,root))return -1;
    struct frame *stack=malloc((size_t)a->count*sizeof(*stack));unsigned char *seen=calloc(a->count,1);
    if(!stack || !seen) {free(stack);free(seen);return -1;}
    uint32_t top=1;stack[0]=(struct frame){root,0};int result=0;
    while(top) {
        struct frame *f=stack+top-1;const struct mip_node *n=a->nodes+f->node;
        if(seen[f->node]) {top--;continue;}
        uint32_t child=f->phase==0?n->left:f->phase==1?n->right:MIP_NO_NODE;
        if(f->phase++<2 && child!=MIP_NO_NODE) {
            if(!seen[child])stack[top++]=(struct frame){child,0};
            continue;
        }
        if(f->phase<=2)continue;
        seen[f->node]=1;result=visit(n,f->node,ctx);top--;if(result)break;
    }
    free(stack);free(seen);return result;
}
struct copy_context {unsigned char *seen;};
static int mark(const struct mip_node *n,uint32_t i,void *p) {(void)n;((struct copy_context *)p)->seen[i]=1;return 0;}
int mip_copy(const struct mip_arena *a,uint32_t root,struct mip_arena *out,uint32_t *result) {
    if(!out || !result || !out->nodes || out->count>out->capacity || valid(a,root) ||
       (out->count && valid(out,out->count-1)))return -1;
    unsigned char *seen=calloc(a->count,1);uint32_t *map=malloc((size_t)a->count*sizeof(*map));
    if(!seen || !map) {free(seen);free(map);return -1;}
    struct copy_context c={seen};int status=mip_walk(a,root,mark,&c);uint32_t count=0;
    for(uint32_t i=0;i<a->count;i++)count+=seen[i]!=0;
    if(status || count>out->capacity-out->count || count>MIP_NODE_LIMIT-out->count ||
       ((uintptr_t)a->nodes<(uintptr_t)(out->nodes+out->capacity) &&
        (uintptr_t)out->nodes<(uintptr_t)(a->nodes+a->count))) {free(seen);free(map);return -1;}
    for(uint32_t i=0;i<a->count;i++)if(seen[i]) {
        struct mip_node n=a->nodes[i];if(n.left!=MIP_NO_NODE)n.left=map[n.left];if(n.right!=MIP_NO_NODE)n.right=map[n.right];
        map[i]=out->count;out->nodes[out->count++]=n;
    }
    *result=map[root];free(seen);free(map);return 0;
}
struct bytes {unsigned char *p;size_t n;};
static void put(struct bytes *b,uint64_t v,unsigned n) {for(unsigned i=0;i<n;i++)b->p[b->n++]=(unsigned char)(v>>(8*i));}
static uint64_t get(struct bytes *b,unsigned n) {uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)b->p[b->n++]<<(8*i);return v;}
int mip_encode(const struct mip_arena *a,uint32_t root,unsigned char **out,size_t *length) {
    if(!out || !length || valid(a,root))return -1;
    size_t size=16;
    for(uint32_t i=0;i<a->count;i++) {size_t n=strlen(a->nodes[i].name);if(n+MIP_WIRE_NODE_SIZE>MIP_WIRE_LIMIT-size)return -1;size+=MIP_WIRE_NODE_SIZE+n;}
    struct bytes b={malloc(size),0};if(!b.p)return -1;
    memcpy(b.p,"NXMIP\0\2\0",8);b.n=8;put(&b,a->count,4);put(&b,root,4);
    for(uint32_t i=0;i<a->count;i++) {
        const struct mip_node *n=a->nodes+i;uint64_t fp;memcpy(&fp,&n->floating,8);
        put(&b,n->op,4);put(&b,n->type,8);put(&b,n->left,4);put(&b,n->right,4);put(&b,n->value,8);put(&b,(uint64_t)n->offset,8);put(&b,fp,8);
        memcpy(b.p+b.n,n->extended,10);b.n+=10;
        put(&b,n->aggregate_size,8);put(&b,n->aggregate_alignment,4);put(&b,n->reg,4);put(&b,n->label,4);put(&b,n->field_size,4);put(&b,n->field_shift,4);put(&b,n->source_line,4);
        size_t len=strlen(n->name);put(&b,len,2);memcpy(b.p+b.n,n->name,len);b.n+=len;
    }
    *out=b.p;*length=b.n;return 0;
}
int mip_decode(const unsigned char *data,size_t length,struct mip_arena *out,uint32_t *result) {
    if(!data || !out || !result || !out->nodes || out->count || length<16 || length>MIP_WIRE_LIMIT || memcmp(data,"NXMIP\0\2\0",8))return -1;
    struct bytes b={(unsigned char *)data,8};uint32_t count=(uint32_t)get(&b,4),root=(uint32_t)get(&b,4);
    if(!count || count>MIP_NODE_LIMIT || count>out->capacity || root>=count || count>(length-16)/MIP_WIRE_NODE_SIZE)return -1;
    struct mip_node *nodes=calloc(count,sizeof(*nodes));if(!nodes)return -1;
    struct mip_arena tmp={nodes,0,count};int status=-1;
    for(uint32_t i=0;i<count;i++) {
        if(length-b.n<MIP_WIRE_NODE_SIZE)goto done;
        struct mip_node n={0};uint64_t fp,offset;uint32_t id;
        n.op=(unsigned)get(&b,4);n.type=get(&b,8);n.left=(uint32_t)get(&b,4);n.right=(uint32_t)get(&b,4);n.value=get(&b,8);offset=get(&b,8);memcpy(&n.offset,&offset,8);
        fp=get(&b,8);memcpy(&n.floating,&fp,8);memcpy(n.extended,data+b.n,10);b.n+=10;n.aggregate_size=get(&b,8);n.aggregate_alignment=(unsigned)get(&b,4);n.reg=(unsigned)get(&b,4);n.label=(unsigned)get(&b,4);
        n.field_size=(unsigned)get(&b,4);n.field_shift=(unsigned)get(&b,4);n.source_line=(unsigned)get(&b,4);size_t len=(size_t)get(&b,2);
        if(len>MIP_NAME_MAX || len>length-b.n)goto done;
        for(size_t j=0;j<len;j++)if(!data[b.n+j])goto done;
        memcpy(n.name,data+b.n,len);b.n+=len;
        if(mip_node_add(&tmp,&n,&id))goto done;
    }
    if(b.n!=length)goto done;
    memcpy(out->nodes,nodes,(size_t)count*sizeof(*nodes));out->count=count;*result=root;status=0;
done:
    free(nodes);return status;
}

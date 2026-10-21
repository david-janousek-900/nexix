/* System V AMD64 aggregate classification from retained front-end layout.
 * The packed class travels in shared IR so a separate backend needs no
 * parser member tables.
 */
#include "mfile1"
struct pcc_va_frame pcc_va_frame;
static unsigned merge(unsigned a,unsigned b) {
    if(a==b || b==PCC_CLASS_NONE)return a;
    if(a==PCC_CLASS_NONE)return b;
    if(a==PCC_CLASS_MEMORY || b==PCC_CLASS_MEMORY || a==PCC_CLASS_X87 || b==PCC_CLASS_X87)return PCC_CLASS_MEMORY;
    if(a==PCC_CLASS_INTEGER || b==PCC_CLASS_INTEGER)return PCC_CLASS_INTEGER;
    return PCC_CLASS_SSE;
}
static void place(unsigned classes[2],uint64_t byte,unsigned class) {
    if(byte>=16) {classes[0]=classes[1]=PCC_CLASS_MEMORY;return;}
    classes[byte/8]=merge(classes[byte/8],class);
}
static void classify(TWORD type,int dim,int size,uint64_t byte,unsigned classes[2],unsigned nesting) {
    if(nesting>MIP_MAX_MODIFIERS+64)cerror("aggregate nesting too deep");
    if(ISARY(type)) {
        TWORD element=DECREF(type);uint64_t count=(uint64_t)dimtab[dim],step=(uint64_t)tsize(element,dim+1,size)/8,i;
        for(i=0;i<count && byte+i*step<16;i++)classify(element,dim+1,size,byte+i*step,classes,nesting+1);
        if(count && byte+count*step>16)place(classes,16,PCC_CLASS_MEMORY);
        return;
    }
    if(ISPTR(type)) {place(classes,byte,PCC_CLASS_INTEGER);return;}
    if(type==STRTY || type==UNIONTY) {
        int i;
        for(i=dimtab[size+1];dimtab[i]>=0;i++) {
            struct symtab *m=stab+dimtab[i];
            if(m->sclass&FIELD) {
                unsigned bits=(unsigned)(m->sclass&FLDSIZ);uint64_t first=(uint64_t)m->offset/8,last=((uint64_t)m->offset+bits-1)/8;
                place(classes,byte+first,PCC_CLASS_INTEGER);if(last/8!=first/8)place(classes,byte+last,PCC_CLASS_INTEGER);
                continue;
            }
            if(m->offset%talign(m->stype,m->sizoff)) {place(classes,16,PCC_CLASS_MEMORY);return;}
            classify(m->stype,m->dimoff,m->sizoff,byte+(uint64_t)m->offset/8,classes,nesting+1);
        }
        return;
    }
    if(type==FLOAT || type==DOUBLE)place(classes,byte,PCC_CLASS_SSE);
    else if(type==LDOUBLE)place(classes,byte,PCC_CLASS_X87);
    else place(classes,byte,PCC_CLASS_INTEGER);
}
unsigned pcc_classify(int size) {
    uint64_t bytes=(uint64_t)tsize(STRTY,0,size)/8;unsigned classes[2]={PCC_CLASS_NONE,PCC_CLASS_NONE};
    if(bytes>16)return PCC_PACK_CLASS(PCC_CLASS_MEMORY,PCC_CLASS_MEMORY);
    classify(STRTY,0,size,0,classes,0);
    /* A lone long double is the X87 return class; it is memory as an argument. */
    if(classes[0]==PCC_CLASS_X87 && classes[1]==PCC_CLASS_NONE && bytes==16)return PCC_PACK_CLASS(PCC_CLASS_X87,PCC_CLASS_NONE);
    if(classes[0]==PCC_CLASS_MEMORY || classes[1]==PCC_CLASS_MEMORY || classes[0]==PCC_CLASS_X87 || classes[1]==PCC_CLASS_X87)
        return PCC_PACK_CLASS(PCC_CLASS_MEMORY,PCC_CLASS_MEMORY);
    if(classes[0]==PCC_CLASS_NONE)classes[0]=PCC_CLASS_INTEGER;
    if(bytes>8 && classes[1]==PCC_CLASS_NONE)classes[1]=PCC_CLASS_INTEGER;
    return PCC_PACK_CLASS(classes[0],bytes>8?classes[1]:PCC_CLASS_NONE);
}

/* LP64 adaptations of the supplied PCC target-dependent front-end helpers. */
#include "mfile1"
NODE *cast(NODE *p,TWORD t) {return makety(p,t,0,(int)BTYPE(t));}
NODE *clocal(NODE *p) {
    NODE *q=p->left;struct mip_layout to,from;
    switch(p->op) {
    case LT:case LE:case GT:case GE:
        if(ISPTR(p->left->type) || ISPTR(p->right->type))p->op+=ULT-LT;
        break;
    case PCONV:case SCONV:
        if(!mip_scalar_layout(p->type,&to) && !mip_scalar_layout(q->type,&from) &&
           p->type!=FLOAT && p->type!=DOUBLE && q->type!=FLOAT && q->type!=DOUBLE) {
            if(q->op==ICON) {
                uint64_t bits=(uint64_t)q->lval;
                if(to.size<8) {
                    unsigned width=(unsigned)to.size*8;uint64_t mask=(UINT64_C(1)<<width)-1;
                    bits&=mask;if(!ISUNSIGNED(p->type) && (bits&(UINT64_C(1)<<(width-1))))bits|=~mask;
                }
                q->lval=(int64_t)bits;q->type=p->type;q->cdim=p->cdim;q->csiz=p->csiz;p->op=FREE;return q;
            }
            if(to.size==from.size && (p->type&TMASK)==(q->type&TMASK)) {
                q->type=p->type;q->cdim=p->cdim;q->csiz=p->csiz;p->op=FREE;return q;
            }
        }
        break;
    case PVCONV:case PMCONV: {
        int op=p->op;if(!p->right || p->right->op!=ICON)cerror("invalid pointer scaling");
        if(op==PVCONV) {p->left->type=LONG;p->left->csiz=LONG;}
        else {
            TWORD type=ISUNSIGNED(p->left->type)?ULONG:LONG;
            p->left=makety(p->left,type,0,(int)type);
        }
        p->op=FREE;return buildtree(op==PMCONV?MUL:DIV,p->left,p->right);
    }
    }
    return p;
}
TWORD ctype(TWORD type) {return type;}
NODE *offcon(OFFSZ off,TWORD type,int d,int s) {
    NODE *p=bcon(0);(void)type;(void)d;(void)s;p->lval=off/SZCHAR;
    if(p->lval<INT32_MIN || p->lval>INT32_MAX)p->type=p->csiz=LONG;
    return p;
}
int andable(NODE *p) {(void)p;return 1;}
int cisreg(TWORD t) {(void)t;return 0;} /* register is a storage hint, not an ABI */
int cendarg(void) {autooff=AUTOINIT;return 0;}
char *exname(char *name) {return name;}
int noinit(void) {return EXTERN;}
int isitfloat(char *s) {dcon=atof(s);return FCON;}

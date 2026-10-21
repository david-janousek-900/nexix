/* Private semantic consumer. Actual instruction emission is tasks 194-195. */
#include "mfile1"
static int location=PROG,label_counter=10;
int getlab(void) {return ++label_counter;}
int locctr(int n) {int old=location;location=n;printf("section %d\n",n);return old;}
int defalign(int n) {printf("align %d\n",n/SZCHAR);return 0;}
int deflab(int n) {printf("label %d\n",n);return 0;}
int branch(int n) {printf("branch %d\n",n);return 0;}
int defnam(struct symtab *p) {printf("object %s type=%lu size=%ld align=%d class=%d\n",p->sname,(unsigned long)p->stype,(long)(tsize(p->stype,p->dimoff,p->sizoff)/8),talign(p->stype,p->sizoff)/8,p->sclass);return 0;}
int bfcode(int *a,int n) {
    struct symtab *p=stab+curftn;int i;printf("function %s type=%lu parameters=%d\n",p->sname,(unsigned long)p->stype,n);retlab=getlab();
    for(i=0;i<n;i++) {p=stab+a[i];printf("parameter %s type=%lu offset=%d\n",p->sname,(unsigned long)p->stype,p->offset/8);}return 0;
}
int bccode(void) {printf("frame %d\n",autooff/8);return 0;}
int efcode(void) {printf("endfunction %s frame=%d\n",stab[curftn].sname,autooff/8);return 0;}
int aobeg(void) {return 0;}
int aocode(struct symtab *p) {printf("automatic %s type=%lu offset=%d\n",p->sname,(unsigned long)p->stype,p->offset/8);return 0;}
int aoend(void) {return 0;}
int ejobcode(int bad) {printf("endunit errors=%d\n",bad);return 0;}
int genswitch(struct sw *p,int n) {int i;printf("switch default=%d count=%d\n",p[0].slab,n);for(i=1;i<=n;i++)printf("case %ld %d\n",(long)p[i].sval,p[i].slab);return 0;}
int fldal(TWORD t) {(void)t;return ALINT;}
int fldty(struct symtab *p) {(void)p;return 0;}
int commdec(int id) {struct symtab *p=stab+id;printf("common %s type=%lu size=%ld align=%d\n",p->sname,(unsigned long)p->stype,(long)tsize(p->stype,p->dimoff,p->sizoff)/8,talign(p->stype,p->sizoff)/8);return 0;}
int bycode(int t,int i) {printf("string-byte %d %d\n",i,t);return 0;}
int zecode(int n) {printf("zero %d\n",n*SZINT/8);inoff+=(OFFSZ)n*SZINT;return 0;}
int vfdzero(int n) {printf("zero-bits %d\n",n);inoff+=n;return 0;}
int incode(NODE *p,int size) {printf("integer-bits %d %lu\n",size,(unsigned long)p->lval);inoff+=size;return 0;}
int fincode(double d,int size) {printf("floating-bits %d %.17g\n",size,d);inoff+=size;return 0;}
int cinit(NODE *p,int size) {ecode(p);inoff+=size;return 0;}
int ecode(NODE *p) {
    if(nerrors)return 0;
    struct mip_node *nodes=calloc(TREESZ,sizeof(*nodes));if(!nodes)cerror("no memory for shared IR");
    struct mip_arena a={nodes,0,TREESZ};uint32_t root=pcc_lower(p,&a);unsigned char *wire=NULL;size_t size=0;
    if(mip_encode(&a,root,&wire,&size))cerror("cannot encode shared IR");
    printf("expression root=%u count=%u bytes=%lu\n",root,a.count,(unsigned long)size);
    unsigned i;for(i=0;i<a.count;i++)printf("node %u %s type=%lu value=%ld offset=%ld name=%s left=%u right=%u\n",i,mip_op_name(nodes[i].op),(unsigned long)nodes[i].type,(long)nodes[i].value,(long)nodes[i].offset,nodes[i].name,nodes[i].left,nodes[i].right);
    free(wire);free(nodes);return 0;
}
int main(int argc,char **argv) {
    if(argc>2) {fprintf(stderr,"usage: pcc-front [preprocessed-source]\n");return 2;}
    if(argc==2) {if(!freopen(argv[1],"r",stdin))return 1;snprintf(ftitle,sizeof(ftitle),"%s",argv[1]);}
    printf("pcc-front abi=1\n");return mainp1(1,argv);
}

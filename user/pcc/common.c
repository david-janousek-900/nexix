/* Native working-tree adapter for the retained PCC semantic front end. */
#include "mfile1"
#include "../mip/mip.h"
int nerrors;
NODE *NIL,*lastfree;
int dope[DSIZE];char *opst[DSIZE];
static void diagnostic(const char *kind,const char *text,va_list ap) {
    fprintf(stderr,"%s:%d: %s",ftitle[0]?ftitle:"<input>",lineno,kind);vfprintf(stderr,text,ap);fputc('\n',stderr);
}
void uerror(const char *text,...) {va_list ap;va_start(ap,text);diagnostic("error: ",text,ap);va_end(ap);if(++nerrors>30)exit(1);}
void werror(const char *text,...) {va_list ap;va_start(ap,text);diagnostic("warning: ",text,ap);va_end(ap);}
void cerror(const char *text,...) {va_list ap;va_start(ap,text);diagnostic("compiler error: ",text,ap);va_end(ap);exit(1);}
tinit() {int i;for(i=0;i<TREESZ;i++)memset(node+i,0,sizeof(*node)),node[i].op=FREE;lastfree=node;}
NODE *talloc(void) {
    unsigned i,start=(unsigned)(lastfree-node);
    for(i=0;i<TREESZ;i++) {unsigned at=(start+i+1)%TREESZ;if(node[at].op==FREE) {memset(node+at,0,sizeof(*node));return lastfree=node+at;}}
    cerror("out of tree space");return NULL;
}
tcheck() {int i;if(!nerrors)for(i=0;i<TREESZ;i++)if(node[i].op!=FREE)cerror("unreleased parse tree");tinit();}
walkf(NODE *p,int (*f)()) {
    NODE *stack[TREESZ];unsigned char phases[TREESZ];unsigned count=1;stack[0]=p;phases[0]=0;
    while(count) {
        NODE *n=stack[count-1];unsigned arity=optype(n->op),phase=phases[count-1]++;
        NODE *child=phase==0 && arity!=LTYPE?n->left:phase==1 && arity==BITYPE?n->right:NULL;
        if(child) {if(count==TREESZ)cerror("tree walk too deep");stack[count]=child;phases[count++]=0;continue;}
        if(phase<2)continue;count--;(*f)(n);
    }
}
static int free_node(NODE *p) {p->op=FREE;return 0;}
tfree(NODE *p) {if(p && p->op!=FREE)walkf(p,free_node);}
fwalk(NODE *p,int (*f)(),int down) {
    int dl=0,dr=0;unsigned arity=optype(p->op);(*f)(p,down,&dl,&dr);
    if(arity!=LTYPE)fwalk(p->left,f,dl);if(arity==BITYPE)fwalk(p->right,f,dr);
}
mkdope() {unsigned i;for(i=0;i<DSIZE;i++) {dope[i]=(int)mip_op_flags(i);opst[i]=(char *)mip_op_name(i);}}
tprint(TWORD t) {printf("type%lu",(unsigned long)t);}

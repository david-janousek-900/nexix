/* Native function signatures, kept outside target IR and parser overlays. */
#include "mfile1"
#define SIGNATURES 1024
#define PARAMETERS 2048
struct parameter {TWORD type;int dim,size;unsigned prototype,next;char *name;};
struct signature {unsigned first,last,count,typed,variadic,result;};
static struct signature signatures[SIGNATURES];
static struct parameter *parameters;
static char *parameter_names;
static size_t name_bytes;
static unsigned signature_count=1,parameter_count=1,stack[32],nesting,definition;
static int saved_class[32];
static struct signature *signature(unsigned id) {if(id>=signature_count)cerror("invalid signature");return signatures+id;}
unsigned pcc_proto_empty(void) {
    if(signature_count==SIGNATURES)cerror("too many function signatures");
    return signature_count++;
}
unsigned pcc_proto_begin(void) {
    if(nesting==32)cerror("prototype nesting too deep");
    unsigned id=pcc_proto_empty();signature(id)->typed=1;
    saved_class[nesting]=curclass;stack[nesting++]=id;return id;
}
void pcc_proto_parameter(NODE *type,NODE *declarator) {
    if(!nesting)cerror("parameter outside prototype");
    NODE *p=tymerge(type,declarator);type->op=FREE;
    struct signature *s=signature(stack[nesting-1]);
    if(s->variadic || s->count==PARAMSZ || parameter_count==PARAMETERS)cerror("too many prototype parameters");
    if(!parameters) {parameters=calloc(PARAMETERS,sizeof(*parameters));parameter_names=malloc(65536);if(!parameters || !parameter_names)cerror("no memory for parameters");}
    struct parameter *v=parameters+parameter_count;v->type=p->type;v->dim=p->cdim;v->size=p->csiz;v->prototype=p->prototype;
    v->name="";
    if(p->rval>=0) {const char *name=stab[p->rval].sname;size_t size=strlen(name)+1;if(size>65536-name_bytes)cerror("too many parameter name bytes");v->name=parameter_names+name_bytes;name_bytes+=size;strcpy(v->name,name);}
    unsigned previous;for(previous=s->first;previous;previous=parameters[previous].next)
        if(v->name[0] && !strcmp(v->name,parameters[previous].name))uerror("duplicate parameter name");
    if(ISARY(v->type)) {v->type=INCREF(DECREF(v->type));v->dim++;}
    else if(ISFTN(v->type))v->type=INCREF(v->type);
    if(s->last)parameters[s->last].next=parameter_count;else s->first=parameter_count;
    s->last=parameter_count++;s->count++;stwart=0;curclass=saved_class[nesting-1];
}
void pcc_proto_variadic(void) {
    if(!nesting)cerror("ellipsis outside prototype");
    struct signature *s=signature(stack[nesting-1]);
    if(!s->count || s->variadic)uerror("invalid variadic prototype");s->variadic=1;
}
unsigned pcc_proto_end(void) {
    if(!nesting)cerror("unbalanced prototype");
    unsigned id=stack[--nesting];struct signature *s=signature(id);
    if(s->count==1 && parameters[s->first].type==VOID && !parameters[s->first].name[0] && !s->variadic)s->first=s->last=s->count=0;
    unsigned i;for(i=s->first;i;i=parameters[i].next)if(parameters[i].type==VOID)uerror("void parameter must stand alone");
    curclass=saved_class[nesting];stwart=0;return id;
}
void pcc_proto_link(unsigned id,unsigned result) {signature(id)->result=result;}
unsigned pcc_proto_result(unsigned id) {return signature(id)->result;}
static int compatible(unsigned a,unsigned b,unsigned depth) {
    if(depth>29)cerror("function signature too deep");
    struct signature *x=signature(a),*y=signature(b);
    if(!x->typed || !y->typed) {
        struct signature *s=x->typed?x:y;unsigned i;
        if(s->variadic)return 0;
        for(i=s->first;i;i=parameters[i].next) {
            TWORD t=parameters[i].type;
            if(t==FLOAT || t==BOOL || t==CHAR || t==UCHAR || t==SHORT || t==USHORT)return 0;
        }
        return 1;
    }
    if(x->count!=y->count || x->variadic!=y->variadic)return 0;
    unsigned i,j;for(i=x->first,j=y->first;i;i=parameters[i].next,j=parameters[j].next) {
        struct parameter *p=parameters+i,*q=parameters+j;
        if(p->type!=q->type || ((p->type==STRTY || p->type==UNIONTY) && p->size!=q->size) || !compatible(p->prototype,q->prototype,depth+1))return 0;
    }
    return compatible(x->result,y->result,depth+1);
}
int pcc_proto_compatible(unsigned a,unsigned b) {if(a==b || !a || !b)return 1;return compatible(a,b,0);}
unsigned pcc_proto_merge(unsigned old,unsigned next) {return signature(next)->typed?next:old?old:next;}
int pcc_typed_definition(void) {return definition && signature(definition)->typed;}
void pcc_proto_definition(unsigned id) {
    definition=id;struct signature *s=signature(id);if(!s->typed)return;
    if(paramno)cerror("mixed prototype and identifier parameters");
    int old=blevel;blevel=1;
    unsigned i;for(i=s->first;i;i=parameters[i].next) {
        struct parameter *v=parameters+i;
        if(!v->name[0]) {uerror("function definition requires parameter names");continue;}
        int name=lookup(v->name,0);ftnarg(name);name=paramstk[paramno-1];
        NODE *p=block(FREE,NIL,NIL,v->type,v->dim,v->size);p->rval=name;p->prototype=v->prototype;defid(p,PARAM);
    }
    blevel=old;
}
static NODE *argument(NODE *n,unsigned *next,unsigned *count,unsigned depth) {
    if(depth>TREESZ)cerror("argument tree too deep");
    if(n->op==CM) {n->left=argument(n->left,next,count,depth+1);n->right=argument(n->right,next,count,depth+1);return n;}
    (*count)++;n=pconvert(n);
    if(*next) {
        struct parameter *p=parameters+*next;*next=p->next;
        if(p->type==STRTY || p->type==UNIONTY) {
            if(n->type!=p->type || n->csiz!=p->size)uerror("incompatible aggregate argument");
        } else {
            if(n->type==STRTY || n->type==UNIONTY || n->type==VOID)uerror("incompatible argument type");
            if(ISPTR(p->type) && !ISPTR(n->type) && !(n->op==ICON && !n->lval))uerror("pointer argument requires pointer or null");
            if(p->prototype && n->prototype && !pcc_proto_compatible(p->prototype,n->prototype))uerror("incompatible callback argument");
            n=makety(n,p->type,p->dim,p->size);n->prototype=p->prototype;
        }
        n->fixed_argument=1;
    }
    return n;
}
NODE *pcc_call_arguments(NODE *function,NODE *arguments) {
    struct signature *s=signature(function->prototype);unsigned next=s->typed?s->first:0,count=0;
    if(arguments)arguments=argument(arguments,&next,&count,0);
    if(s->typed && (next || (!s->variadic && count!=s->count)))uerror("wrong number of function arguments");
    return arguments;
}

static int va_object(NODE *p) {
    return p && p->type==PTR+STRTY && tsize(STRTY,p->cdim,p->csiz)==192;
}
NODE *pcc_vararg(unsigned op,NODE *ap,NODE *other,NODE *type) {
    ap=pconvert(ap);if(!va_object(ap))uerror("va_list object required");
    if(op==VA_COPY) {other=pconvert(other);if(!va_object(other))uerror("va_copy source requires va_list");}
    if(op==VA_START) {
        struct signature *s=signature(definition);
        if(!s->typed || !s->variadic)uerror("va_start requires variadic function");
        if(!other || other->op!=NAME || other->rval<0 || other->rval>=SYMTSZ || stab[other->rval].sclass!=PARAM || !s->last || strcmp(stab[other->rval].sname,parameters[s->last].name))uerror("va_start requires last named parameter");
    }
    TWORD result=type?type->type:VOID;int dim=type?type->cdim:0,size=type?type->csiz:VOID;
    if(type && (result==VOID || ISFTN(result) || ISARY(result)))uerror("invalid va_arg type");
    NODE *p=block((int)op,ap,other,result,dim,size);
    if(type) {p->prototype=type->prototype;type->op=FREE;}
    return p;
}

void pcc_align_type(NODE *type,int alignment) {
    if(alignment<1 || alignment>4096 || (alignment&(alignment-1))) {uerror("invalid alignment");return;}
    if(type->type!=STRTY && type->type!=UNIONTY) {uerror("aggregate alignment required");return;}
    int size=type->csiz,bits=alignment*8;
    if(bits>dimtab[size+2])dimtab[size+2]=bits;
    if(dimtab[size]>INT32_MAX-(bits-1))cerror("aligned type too large");
    dimtab[size]=(dimtab[size]+bits-1)&~(bits-1);
}

void pcc_check_definition(const int *args,int count) {
    struct signature *s=signature(stab[curftn].prototype);
    if(!s->typed || pcc_typed_definition())return;
    if(s->variadic || s->count!=(unsigned)count) {uerror("definition conflicts with prototype");return;}
    unsigned i=s->first;int j;
    for(j=0;j<count;j++,i=parameters[i].next) {
        struct symtab *p=stab+args[j];TWORD promoted=p->stype==FLOAT?DOUBLE:pcc_promote(p->stype);
        if(promoted!=parameters[i].type)uerror("parameter definition conflicts with prototype");
    }
}

/* Conservative AMD64 integer/pointer generation from shared native mip IR.
 * Values live in RAX, binary RHS in RCX; other live values spill to the stack.
 * All three hardware scratch registers are described to mip's allocator.
 */
#include "backend.h"
static const struct mip_arena *current_ir;
static unsigned depth;
static const struct mip_node *node_at(uint32_t i) {if(i>=current_ir->count)cerror("invalid target node");return current_ir->nodes+i;}
static void push(const char *reg) {printf(" pushq %%%s\n",reg);depth++;}
static void pop(const char *reg) {if(!depth)cerror("target stack underflow");printf(" popq %%%s\n",reg);depth--;}
static unsigned width(TWORD type) {
    struct mip_layout l;
    if(type==FLOAT || type==DOUBLE || mip_scalar_layout(type,&l))cerror("non-integer expression requires floating/aggregate backend");
    return (unsigned)l.size;
}
static void normalize(TWORD type) {
    unsigned n=width(type);int uns=ISUNSIGNED(type);
    if(n==1)puts(uns?" movzbq %al,%rax":" movsbq %al,%rax");
    else if(n==2)puts(uns?" movzwq %ax,%rax":" movswq %ax,%rax");
    else if(n==4)puts(uns?" movl %eax,%eax":" movslq %eax,%rax");
}
static void symbol(const char *name,int64_t addend) {
    printf("%s",name);if(addend>0)printf("+%lu",(unsigned long)addend);
    else if(addend<0)printf("-%lu",(unsigned long)(0-(uint64_t)addend));
}
static void generate(uint32_t,unsigned);
static void address(uint32_t i,unsigned nesting) {
    const struct mip_node *n=node_at(i);
    if(n->op==UNARY MUL) {generate(n->left,nesting+1);return;}
    if(n->op==FLD) {address(n->left,nesting+1);return;}
    if(n->op!=NAME && n->op!=ICON)cerror("expression is not addressable");
    uint64_t bits=(uint64_t)n->offset+n->value;int64_t offset=(int64_t)bits;
    if(n->reg==AUTO || n->reg==PARAM) {
        if(offset>=INT32_MIN && offset<=INT32_MAX)printf(" leaq %ld(%%rbp),%%rax\n",(long)offset);
        else printf(" movq $%ld,%%rax\n addq %%rbp,%%rax\n",(long)offset);
    } else {
        if(!n->name[0])cerror("missing address symbol");
        printf(" movabsq $");symbol(n->name,(int64_t)n->value);puts(",%rax");
    }

}
static void load(TWORD type) {
    unsigned n=width(type);int uns=ISUNSIGNED(type);
    if(n==1)puts(uns?" movzbq (%rax),%rax":" movsbq (%rax),%rax");
    else if(n==2)puts(uns?" movzwq (%rax),%rax":" movswq (%rax),%rax");
    else if(n==4)puts(uns?" movl (%rax),%eax":" movslq (%rax),%rax");
    else puts(" movq (%rax),%rax");
}
static void field_normalize(const struct mip_node *n) {
    unsigned shift=64-n->field_size;
    if(shift)printf(" shlq $%u,%%rax\n %s $%u,%%rax\n",shift,ISUNSIGNED(n->type)?"shrq":"sarq",shift);
}
static unsigned field_bytes(const struct mip_node *n) {
    unsigned total=n->field_shift+n->field_size;
    if(!n->field_size || total>32)cerror("unsupported integer field width");
    return total<=8?1:total<=16?2:4;
}
static void load_left(uint32_t left) {
    const struct mip_node *n=node_at(left);
    if(n->op!=FLD) {load(n->type);return;}
    unsigned bytes=field_bytes(n);
    puts(bytes==1?" movzbq (%rax),%rax":bytes==2?" movzwq (%rax),%rax":" movl (%rax),%eax");
    if(n->field_shift)printf(" shrq $%u,%%rax\n",n->field_shift);
    field_normalize(n);
}
static void store(uint32_t left) {
    const struct mip_node *n=node_at(left);
    if(n->op==FLD) {
        unsigned bytes=field_bytes(n);
        uint64_t mask=(UINT64_C(1)<<n->field_size)-1;
        printf(" movq $%lu,%%r10\n andq %%r10,%%rax\n movq %%rax,%%r10\n",(unsigned long)mask);
        if(n->field_shift)printf(" shlq $%u,%%r10\n",n->field_shift);
        printf(" movq $%lu,%%r11\n",(unsigned long)~(mask<<n->field_shift));
        puts(bytes==1?" movzbq (%rcx),%rdx":bytes==2?" movzwq (%rcx),%rdx":" movl (%rcx),%edx");
        puts(" andq %r11,%rdx\n orq %r10,%rdx");
        printf(" mov%s %%%s,(%%rcx)\n",bytes==1?"b":bytes==2?"w":"l",bytes==1?"dl":bytes==2?"dx":"edx");
        field_normalize(n);return;
    }
    unsigned nbytes=width(n->type);
    printf(" mov%s %%%s,(%%rcx)\n",nbytes==1?"b":nbytes==2?"w":nbytes==4?"l":"q",nbytes==1?"al":nbytes==2?"ax":nbytes==4?"eax":"rax");
}
static void binary_instruction(unsigned op,TWORD type) {
    int uns=ISUNSIGNED(type) || (type&TMASK);const char *condition=NULL;
    switch(op) {
    case PLUS:puts(" addq %rcx,%rax");break;case MINUS:puts(" subq %rcx,%rax");break;
    case MUL:puts(" imulq %rcx,%rax");break;
    case DIV:case MOD:
        puts(uns?" xorl %edx,%edx\n divq %rcx":" cqto\n idivq %rcx");if(op==MOD)puts(" movq %rdx,%rax");break;
    case AND:puts(" andq %rcx,%rax");break;case OR:puts(" orq %rcx,%rax");break;case ER:puts(" xorq %rcx,%rax");break;
    case LS:puts(" shlq %cl,%rax");break;case RS:case ARS:puts(uns?" shrq %cl,%rax":" sarq %cl,%rax");break;
    case EQ:condition="e";break;case NE:condition="ne";break;
    case LT:condition=uns?"b":"l";break;case LE:condition=uns?"be":"le";break;
    case GT:condition=uns?"a":"g";break;case GE:condition=uns?"ae":"ge";break;
    case ULT:condition="b";break;case ULE:condition="be";break;case UGT:condition="a";break;case UGE:condition="ae";break;
    default:cerror("unsupported integer operator %u",op);
    }
    if(condition)printf(" cmpq %%rcx,%%rax\n set%s %%al\n movzbq %%al,%%rax\n",condition);
}
struct binary_context {unsigned op;TWORD type;};
static int expand(void *p,char escape,unsigned modifier,const struct mip_node *n) {
    (void)n;struct binary_context *c=p;
    if(!escape)return fputc((int)modifier,stdout)==EOF;
    if(escape!='Z' || modifier!='b')return -1;
    binary_instruction(c->op,c->type);return ferror(stdout)!=0;
}
static void binary(uint32_t root,unsigned op,TWORD type) {
    static const struct mip_register registers[]={ {"rax",SAREG|STAREG,1},{"rcx",SAREG|STAREG,2},{"rdx",SAREG|STAREG,4} };
    static const struct mip_template table[]={
        {OPSIMP,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {ASG OPSIMP,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {OPMUL,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {ASG OPMUL,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {OPDIV,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {ASG OPDIV,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {OPSHFT,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {ASG OPSHFT,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {OPLOG,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {INCR,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {DECR,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"},
        {ARS,INAREG,SANY,TANY,SANY,TANY,NAREG,RESC1,"Zb"}
    };
    struct mip_resources resources;struct mip_allocation allocation;size_t selected;
    if(mip_resources_init(&resources,registers,3,0) || mip_register_hold(&resources,0) || mip_register_hold(&resources,1) ||
       mip_match(current_ir,root,INAREG,table,sizeof(table)/sizeof(*table),&resources,&selected,&allocation)!=1 || allocation.count!=1 || allocation.regs[0]!=2)cerror("integer table/resource failure");
    struct binary_context context={op,type};
    if(mip_expand(current_ir,root,INAREG,table[selected].text,NULL,expand,&context))cerror("integer template expansion failed");
}
static void assignment(uint32_t root,unsigned nesting) {
    const struct mip_node *n=node_at(root),*left=node_at(n->left);int post=n->op==INCR || n->op==DECR;
    address(n->left,nesting+1);push("rax");
    if(n->op!=ASSIGN) {
        load_left(n->left);push("rax");
        generate(n->right,nesting+1);puts(" movq %rax,%rcx");
        if(post)puts(" movq (%rsp),%rax");else pop("rax");
        unsigned op=n->op==INCR?PLUS:n->op==DECR?MINUS:n->op-1;
        TWORD arithmetic=left->type;
        if(!(arithmetic&TMASK)) {
            arithmetic=left->op==FLD && left->type==UNSIGNED && left->field_size<32?INT:pcc_promote(arithmetic);
            if(op!=LS && op!=RS)arithmetic=pcc_arithmetic(arithmetic,node_at(n->right)->type);
        }
        binary(root,op,arithmetic);normalize(left->type);
    } else {generate(n->right,nesting+1);normalize(left->type);}
    if(post) {
        puts(" movq 8(%rsp),%rcx");store(n->left);pop("rax");pop("r10");
    } else {pop("rcx");store(n->left);}
}
static void arguments(uint32_t root,uint32_t *items,unsigned *count,unsigned nesting) {
    if(nesting>TREESZ)cerror("call argument tree too deep");
    const struct mip_node *n=node_at(root);
    if(n->op==CM) {arguments(n->left,items,count,nesting+1);arguments(n->right,items,count,nesting+1);return;}
    if(*count>=256)cerror("too many call arguments");
    if(n->op==STARG || n->type==FLOAT || n->type==DOUBLE || n->type==STRTY || n->type==UNIONTY)
        cerror("call argument requires floating/aggregate ABI");
    items[(*count)++]=root;
}
static void call(const struct mip_node *n,unsigned nesting) {
    static const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
    uint32_t items[256];unsigned count=0,i;
    if(n->op==CALL)arguments(n->right,items,&count,nesting+1);
    unsigned stack=count>6?count-6:0,slots=count+1;
    if((depth+slots)&1)slots++;
    printf(" subq $%u,%%rsp\n",slots*8);depth+=slots;
    generate(n->left,nesting+1);printf(" movq %%rax,%u(%%rsp)\n",count*8);
    for(i=0;i<count;i++) {
        generate(items[i],nesting+1);
        unsigned slot=i<6?stack+i:i-6;
        printf(" movq %%rax,%u(%%rsp)\n",slot*8);
    }
    for(i=0;i<count && i<6;i++)printf(" movq %u(%%rsp),%%%s\n",(stack+i)*8,regs[i]);
    printf(" movq %u(%%rsp),%%r11\n xorl %%eax,%%eax\n call *%%r11\n addq $%u,%%rsp\n",count*8,slots*8);
    depth-=slots;
}
static void generate(uint32_t root,unsigned nesting) {
    if(nesting>TREESZ)cerror("target expression too deep");
    const struct mip_node *n=node_at(root);unsigned op=n->op;unsigned initial=depth;
    switch(op) {
    case CALL:case UNARY CALL:call(n,nesting+1);break;
    case ICON:
        if(n->name[0])address(root,nesting+1);else printf(" movq $%ld,%%rax\n",(long)n->value);
        normalize(n->type);break;
    case NAME:
        address(root,nesting+1);if(!ISARY(n->type) && !ISFTN(n->type))load(n->type);break;
    case UNARY AND:address(n->left,nesting+1);break;
    case UNARY MUL:address(root,nesting+1);load(n->type);break;
    case FLD:
        address(root,nesting+1);load_left(root);break;
    case PCONV:case SCONV:generate(n->left,nesting+1);normalize(n->type);break;
    case FORCE:generate(n->left,nesting+1);normalize(n->type);break;
    case COMOP:case CM:generate(n->left,nesting+1);generate(n->right,nesting+1);break;
    case UNARY MINUS:case COMPL:case NOT:
        generate(n->left,nesting+1);
        if(op==NOT)puts(" testq %rax,%rax\n sete %al\n movzbq %al,%rax");
        else {puts(op==COMPL?" notq %rax":" negq %rax");normalize(n->type);}break;
    case ANDAND:case OROR: {
        int shortcut=getlab(),end=getlab();generate(n->left,nesting+1);
        printf(" testq %%rax,%%rax\n j%s .L%d\n",op==ANDAND?"z":"nz",shortcut);
        generate(n->right,nesting+1);printf(" testq %%rax,%%rax\n setne %%al\n movzbq %%al,%%rax\n jmp .L%d\n",end);
        deflab(shortcut);printf(" movq $%u,%%rax\n",op==OROR);deflab(end);break;
    }
    case QUEST: {
        const struct mip_node *arms=node_at(n->right);if(arms->op!=COLON)cerror("bad conditional tree");
        int alternate=getlab(),end=getlab();generate(n->left,nesting+1);printf(" testq %%rax,%%rax\n jz .L%d\n",alternate);
        generate(arms->left,nesting+1);printf(" jmp .L%d\n",end);deflab(alternate);generate(arms->right,nesting+1);deflab(end);break;
    }
    case CBRANCH:
        generate(n->left,nesting+1);printf(" testq %%rax,%%rax\n jz .L%lu\n",(unsigned long)node_at(n->right)->value);break;
    case GOTO:generate(n->left,nesting+1);puts(" jmp *%rax");break;
    case ASSIGN:case INCR:case DECR:case ASG PLUS:case ASG MINUS:case ASG MUL:case ASG DIV:case ASG MOD:
    case ASG AND:case ASG OR:case ASG ER:case ASG LS:case ASG RS:assignment(root,nesting+1);break;
    case PLUS:case MINUS:case MUL:case DIV:case MOD:case AND:case OR:case ER:case LS:case RS:case ARS:
    case EQ:case NE:case LT:case LE:case GT:case GE:case ULT:case ULE:case UGT:case UGE:
        generate(n->left,nesting+1);push("rax");generate(n->right,nesting+1);puts(" movq %rax,%rcx");pop("rax");
        binary(root,op,node_at(n->left)->type);normalize(n->type);break;
    default:cerror("operator %u requires remaining backend work",op);
    }
    if(op!=CBRANCH && op!=GOTO && !ISARY(n->type) && !ISFTN(n->type))normalize(n->type);
    if(depth!=initial)cerror("unbalanced target expression stack");
}
void pcc_emit_expression(const struct mip_arena *arena,uint32_t root) {current_ir=arena;depth=0;generate(root,0);current_ir=NULL;}
struct constant {uint64_t value;const char *symbol;TWORD type;};
static struct constant constant(uint32_t i,unsigned nesting) {
    if(nesting>TREESZ)cerror("initializer too deep");const struct mip_node *n=node_at(i);
    if(n->op==INIT || n->op==SCONV || n->op==PCONV) {struct constant c=constant(n->left,nesting+1);c.type=n->type;if(!c.symbol)c.value=pcc_integer_bits(c.value,c.type);return c;}
    if(n->op==ICON)return (struct constant){n->value,n->name[0]?n->name:NULL,n->type};
    if((mip_op_flags(n->op)&TYFLG)==BITYPE) {
        struct constant a=constant(n->left,nesting+1),b=constant(n->right,nesting+1);
        if(a.symbol || b.symbol) {
            if(n->op==PLUS && !(a.symbol && b.symbol))return (struct constant){a.value+b.value,a.symbol?a.symbol:b.symbol,n->type};
            if(n->op==MINUS && a.symbol && !b.symbol)return (struct constant){a.value-b.value,a.symbol,n->type};
            if(n->op==MINUS && a.symbol && b.symbol && !strcmp(a.symbol,b.symbol))return (struct constant){a.value-b.value,NULL,n->type};
            cerror("unsupported symbolic initializer");
        }
        NODE l={0},r={0};l.op=r.op=ICON;l.type=a.type;r.type=b.type;l.lval=(int64_t)a.value;r.lval=(int64_t)b.value;l.rval=r.rval=NONAME;
        if(!pcc_conval(&l,(int)n->op,&r))cerror("nonconstant initializer");
        return (struct constant){pcc_integer_bits((uint64_t)l.lval,n->type),NULL,n->type};
    }
    cerror("nonconstant initializer node %u",n->op);return (struct constant){0,NULL,INT};
}
void pcc_emit_initializer(const struct mip_arena *arena,uint32_t root,unsigned bits) {
    current_ir=arena;struct constant value=constant(root,0);
    if(bits!=32 && bits!=64)cerror("invalid scalar initializer width");
    printf(bits==64?".quad ":".long ");
    if(value.symbol)symbol(value.symbol,(int64_t)value.value);
    else printf("%lu",bits==64?(unsigned long)value.value:(unsigned long)(uint32_t)value.value);
    putchar('\n');current_ir=NULL;
}

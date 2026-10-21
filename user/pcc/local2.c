/* Conservative AMD64 integer/pointer generation from shared native mip IR.
 * Values live in RAX, binary RHS in RCX; other live values spill to the stack.
 * All three hardware scratch registers are described to mip's allocator.
 */
#include "backend.h"
static const struct mip_arena *current_ir;
static unsigned depth;
static int floating(TWORD type) {return type==FLOAT || type==DOUBLE;}
static const struct mip_node *node_at(uint32_t i) {if(i>=current_ir->count)cerror("invalid target node");return current_ir->nodes+i;}
static void push(const char *reg) {printf(" pushq %%%s\n",reg);depth++;}
static void pop(const char *reg) {if(!depth)cerror("target stack underflow");printf(" popq %%%s\n",reg);depth--;}
static unsigned width(TWORD type) {
    struct mip_layout l;
    if(type==FLOAT || type==DOUBLE || mip_scalar_layout(type,&l))cerror("non-integer expression requires floating/aggregate backend");
    return (unsigned)l.size;
}
static void normalize(TWORD type) {
    if(floating(type))return;
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
    if(floating(type)) {printf(" mov%s (%%rax),%%xmm0\n",type==FLOAT?"ss":"sd");return;}
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
    if(floating(n->type)) {printf(" mov%s %%xmm0,(%%rcx)\n",n->type==FLOAT?"ss":"sd");return;}
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
static void spill_float(void) {puts(" subq $8,%rsp\n movq %xmm0,(%rsp)");depth++;}
static void restore_float(void) {puts(" movq (%rsp),%xmm0\n addq $8,%rsp");depth--;}
static void scalar_convert(TWORD from,TWORD to) {
    if(from==to)return;
    if(floating(from) && floating(to)) {puts(to==FLOAT?" cvtsd2ss %xmm0,%xmm0":" cvtss2sd %xmm0,%xmm0");return;}
    if(floating(to)) {
        if(from==ULONG) {
            int ordinary=getlab(),end=getlab();
            printf(" testq %%rax,%%rax\n jns .L%d\n movq %%rax,%%rcx\n andl $1,%%eax\n shrq $1,%%rcx\n orq %%rcx,%%rax\n",ordinary);
            printf(" cvtsi2%sq %%rax,%%xmm0\n add%s %%xmm0,%%xmm0\n jmp .L%d\n",to==FLOAT?"ss":"sd",to==FLOAT?"ss":"sd",end);
            deflab(ordinary);printf(" cvtsi2%sq %%rax,%%xmm0\n",to==FLOAT?"ss":"sd");deflab(end);
        } else printf(" cvtsi2%sq %%rax,%%xmm0\n",to==FLOAT?"ss":"sd");
        return;
    }
    if(floating(from)) {
        const char *s=from==FLOAT?"ss":"sd";
        if(to==ULONG) {
            int small=getlab(),end=getlab();uint64_t bits=from==FLOAT?UINT64_C(0x5f000000):UINT64_C(0x43e0000000000000);
            printf(" movq $%lu,%%rax\n movq %%rax,%%xmm1\n ucomi%s %%xmm1,%%xmm0\n jb .L%d\n sub%s %%xmm1,%%xmm0\n cvtt%s2siq %%xmm0,%%rax\n movq $-9223372036854775808,%%rcx\n xorq %%rcx,%%rax\n jmp .L%d\n",(unsigned long)bits,s,small,s,s,end);
            deflab(small);printf(" cvtt%s2siq %%xmm0,%%rax\n",s);deflab(end);
        } else printf(" cvtt%s2siq %%xmm0,%%rax\n",s);
        normalize(to);
    }
}
static void truth(TWORD type) {
    if(floating(type)) {
        printf(" pxor %%xmm1,%%xmm1\n ucomi%s %%xmm1,%%xmm0\n setne %%al\n setp %%cl\n orb %%cl,%%al\n movzbq %%al,%%rax\n",type==FLOAT?"ss":"sd");
    }
}
static void float_binary(unsigned op,TWORD type) {
    const char *s=type==FLOAT?"ss":"sd",*instruction=NULL,*condition=NULL;
    switch(op) {
    case PLUS:instruction="add";break;case MINUS:instruction="sub";break;
    case MUL:instruction="mul";break;case DIV:instruction="div";break;
    case EQ:condition="e";break;case NE:condition="ne";break;
    case LT:condition="b";break;case LE:condition="be";break;
    case GT:condition="a";break;case GE:condition="ae";break;
    default:cerror("unsupported floating operator %u",op);
    }
    if(instruction)printf(" %s%s %%xmm1,%%xmm0\n",instruction,s);
    else {
        printf(" ucomi%s %%xmm1,%%xmm0\n set%s %%al\n",s,condition);
        if(op==NE)puts(" setp %cl\n orb %cl,%al");
        else puts(" setnp %cl\n andb %cl,%al");
        puts(" movzbq %al,%rax");
    }
}
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
    if(floating(left->type) || floating(node_at(n->right)->type)) {
        if(n->op==ASSIGN) {generate(n->right,nesting+1);scalar_convert(node_at(n->right)->type,left->type);}
        else {
            TWORD arithmetic=pcc_arithmetic(left->type,node_at(n->right)->type);
            load(left->type);if(post)spill_float();scalar_convert(left->type,arithmetic);spill_float();
            generate(n->right,nesting+1);scalar_convert(node_at(n->right)->type,arithmetic);
            puts(" movq %xmm0,%xmm1");restore_float();
            float_binary(n->op==INCR?PLUS:n->op==DECR?MINUS:n->op-1,arithmetic);scalar_convert(arithmetic,left->type);
        }
        if(post) {puts(" movq 8(%rsp),%rcx");store(n->left);restore_float();pop("r10");}
        else {pop("rcx");store(n->left);}
        return;
    }
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
    if(n->op==STARG || n->type==STRTY || n->type==UNIONTY)
        cerror("call argument requires floating/aggregate ABI");
    items[(*count)++]=root;
}
static void call(const struct mip_node *n,unsigned nesting) {
    static const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
    uint32_t items[256];unsigned count=0,i,ngp=0,nsse=0,stack=0,slot[256],reg[256];
    if(n->op==CALL)arguments(n->right,items,&count,nesting+1);
    for(i=0;i<count;i++) {
        if(floating(node_at(items[i])->type))reg[i]=nsse<8?nsse++:UINT32_MAX;
        else reg[i]=ngp<6?ngp++:UINT32_MAX;
        if(reg[i]==UINT32_MAX)slot[i]=stack++;
    }
    unsigned next=stack,slots=count+1;
    for(i=0;i<count;i++)if(reg[i]!=UINT32_MAX)slot[i]=next++;
    if((depth+slots)&1)slots++;
    printf(" subq $%u,%%rsp\n",slots*8);depth+=slots;
    generate(n->left,nesting+1);printf(" movq %%rax,%u(%%rsp)\n",count*8);
    for(i=0;i<count;i++) {
        generate(items[i],nesting+1);
        printf(" movq %%%s,%u(%%rsp)\n",floating(node_at(items[i])->type)?"xmm0":"rax",slot[i]*8);
    }
    for(i=0;i<count;i++)if(reg[i]!=UINT32_MAX) {
        if(floating(node_at(items[i])->type))printf(" movq %u(%%rsp),%%xmm%u\n",slot[i]*8,reg[i]);
        else printf(" movq %u(%%rsp),%%%s\n",slot[i]*8,regs[reg[i]]);
    }
    printf(" movq %u(%%rsp),%%r11\n movl $%u,%%eax\n call *%%r11\n addq $%u,%%rsp\n",count*8,nsse,slots*8);
    depth-=slots;
}
static void generate(uint32_t root,unsigned nesting) {
    if(nesting>TREESZ)cerror("target expression too deep");
    const struct mip_node *n=node_at(root);unsigned op=n->op;unsigned initial=depth;
    switch(op) {
    case CALL:case UNARY CALL:call(n,nesting+1);break;
    case FCON: {
        uint64_t bits;
        if(n->type==FLOAT) {float f=(float)n->floating;uint32_t v;memcpy(&v,&f,4);bits=v;}
        else memcpy(&bits,&n->floating,8);
        printf(" movq $%lu,%%rax\n movq %%rax,%%xmm0\n",(unsigned long)bits);break;
    }
    case ICON:
        if(n->name[0])address(root,nesting+1);else printf(" movq $%ld,%%rax\n",(long)n->value);
        normalize(n->type);break;
    case NAME:
        address(root,nesting+1);if(!ISARY(n->type) && !ISFTN(n->type))load(n->type);break;
    case UNARY AND:address(n->left,nesting+1);break;
    case UNARY MUL:address(root,nesting+1);load(n->type);break;
    case FLD:
        address(root,nesting+1);load_left(root);break;
    case PCONV:case SCONV:case FORCE:generate(n->left,nesting+1);scalar_convert(node_at(n->left)->type,n->type);normalize(n->type);break;
    case COMOP:case CM:generate(n->left,nesting+1);generate(n->right,nesting+1);break;
    case UNARY MINUS:case COMPL:case NOT:
        generate(n->left,nesting+1);
        if(op==NOT) {truth(node_at(n->left)->type);puts(" testq %rax,%rax\n sete %al\n movzbq %al,%rax");}
        else if(floating(n->type) && op==UNARY MINUS) {
            printf(" movq $%lu,%%rax\n movq %%rax,%%xmm1\n pxor %%xmm1,%%xmm0\n",n->type==FLOAT?UINT64_C(0x80000000):UINT64_C(0x8000000000000000));
        }
        else {puts(op==COMPL?" notq %rax":" negq %rax");normalize(n->type);}break;
    case ANDAND:case OROR: {
        int shortcut=getlab(),end=getlab();generate(n->left,nesting+1);truth(node_at(n->left)->type);
        printf(" testq %%rax,%%rax\n j%s .L%d\n",op==ANDAND?"z":"nz",shortcut);
        generate(n->right,nesting+1);truth(node_at(n->right)->type);printf(" testq %%rax,%%rax\n setne %%al\n movzbq %%al,%%rax\n jmp .L%d\n",end);
        deflab(shortcut);printf(" movq $%u,%%rax\n",op==OROR);deflab(end);break;
    }
    case QUEST: {
        const struct mip_node *arms=node_at(n->right);if(arms->op!=COLON)cerror("bad conditional tree");
        int alternate=getlab(),end=getlab();generate(n->left,nesting+1);truth(node_at(n->left)->type);printf(" testq %%rax,%%rax\n jz .L%d\n",alternate);
        generate(arms->left,nesting+1);printf(" jmp .L%d\n",end);deflab(alternate);generate(arms->right,nesting+1);deflab(end);break;
    }
    case CBRANCH:
        generate(n->left,nesting+1);truth(node_at(n->left)->type);printf(" testq %%rax,%%rax\n jz .L%lu\n",(unsigned long)node_at(n->right)->value);break;
    case GOTO:generate(n->left,nesting+1);puts(" jmp *%rax");break;
    case ASSIGN:case INCR:case DECR:case ASG PLUS:case ASG MINUS:case ASG MUL:case ASG DIV:case ASG MOD:
    case ASG AND:case ASG OR:case ASG ER:case ASG LS:case ASG RS:assignment(root,nesting+1);break;
    case PLUS:case MINUS:case MUL:case DIV:case MOD:case AND:case OR:case ER:case LS:case RS:case ARS:
    case EQ:case NE:case LT:case LE:case GT:case GE:case ULT:case ULE:case UGT:case UGE:
        if(floating(node_at(n->left)->type)) {
            generate(n->left,nesting+1);spill_float();generate(n->right,nesting+1);
            puts(" movq %xmm0,%xmm1");restore_float();float_binary(op,node_at(n->left)->type);break;
        }
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

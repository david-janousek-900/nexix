/* Conservative AMD64 integer/pointer generation from shared native mip IR.
 * Values live in RAX, binary RHS in RCX; other live values spill to the stack.
 * All three hardware scratch registers are described to mip's allocator.
 */
#include "backend.h"
static const struct mip_arena *current_ir;
static unsigned depth;
/* Parser labels use the lower half. Backend-only labels have their own
 * sequence so a serialized frontend never needs to run code generation. */
static int backend_label=0x40000000;
static int target_label(void) {if(backend_label==INT32_MAX)cerror("too many target labels");return ++backend_label;}
static int floating(TWORD type) {return type==FLOAT || type==DOUBLE;}
static int aggregate(TWORD type) {return type==STRTY || type==UNIONTY;}
static int extended(TWORD type) {return type==LDOUBLE;}
static const struct mip_node *node_at(uint32_t i) {if(i>=current_ir->count)cerror("invalid target node");return current_ir->nodes+i;}
static void push(const char *reg) {printf(" pushq %%%s\n",reg);depth++;}
static void pop(const char *reg) {if(!depth)cerror("target stack underflow");printf(" popq %%%s\n",reg);depth--;}
static unsigned width(TWORD type) {
    struct mip_layout l;
    if(type==FLOAT || type==DOUBLE || mip_scalar_layout(type,&l))cerror("non-integer expression requires floating/aggregate backend");
    return (unsigned)l.size;
}
static void normalize(TWORD type) {
    if(type==VOID || floating(type) || aggregate(type))return;
    if(type==LDOUBLE)return;
    if(type==BOOL) {puts(" testq %rax,%rax\n setne %al\n movzbq %al,%rax");return;}
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
    if(n->op==VA_ARG || n->op==STCALL || n->op==UNARY STCALL || n->op==STARG) {generate(i,nesting+1);return;}
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
    if(extended(type)) {puts(" fldt (%rax)");return;}
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
    if(extended(n->type)) {puts(" fld %st(0)\n fstpt (%rcx)");return;}
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
/* The x87 accumulator holds exactly one long double; spills pop it. */
static void spill_extended(void) {puts(" subq $16,%rsp\n fstpt (%rsp)");depth+=2;}
static void restore_extended(void) {puts(" fldt (%rsp)\n addq $16,%rsp");depth-=2;}
static void spill_value(TWORD type) {if(extended(type))spill_extended();else spill_float();}
static void keep_value(TWORD type) {if(extended(type))puts(" fld %st(0)");spill_value(type);}
static void restore_value(TWORD type) {if(extended(type))restore_extended();else restore_float();}
static unsigned spill_bytes(TWORD type) {return extended(type)?16:8;}
static void discard(TWORD type) {if(extended(type))puts(" fstp %st(0)");}
/* Truncating x87 conversion to a signed 64-bit integer in RAX. */
static void extended_truncate(void) {
    puts(" subq $16,%rsp\n fnstcw (%rsp)\n movzwq (%rsp),%rax\n orq $3072,%rax\n movw %ax,2(%rsp)\n fldcw 2(%rsp)\n fistpq 8(%rsp)\n fldcw (%rsp)\n movq 8(%rsp),%rax\n addq $16,%rsp");
}
static void truth(TWORD);
static void scalar_convert(TWORD from,TWORD to) {
    if(to==VOID) {discard(from);return;}
    if(to==BOOL && (floating(from) || extended(from))) {truth(from);return;}
    if(from==to)return;
    if(extended(to)) {
        if(floating(from))printf(" subq $8,%%rsp\n mov%s %%xmm0,(%%rsp)\n fld%s (%%rsp)\n addq $8,%%rsp\n",from==FLOAT?"ss":"sd",from==FLOAT?"s":"l");
        else if(from==ULONG || from==ULLONG || (from&TMASK)) {
            int done=target_label();
            printf(" subq $8,%%rsp\n movq %%rax,(%%rsp)\n fildq (%%rsp)\n testq %%rax,%%rax\n jns .L%d\n movl $1602224128,(%%rsp)\n flds (%%rsp)\n faddp\n",done);
            deflab(done);puts(" addq $8,%rsp");
        } else puts(" subq $8,%rsp\n movq %rax,(%rsp)\n fildq (%rsp)\n addq $8,%rsp");
        return;
    }
    if(extended(from)) {
        if(floating(to)) {printf(" subq $8,%%rsp\n fstp%s (%%rsp)\n mov%s (%%rsp),%%xmm0\n addq $8,%%rsp\n",to==FLOAT?"s":"l",to==FLOAT?"ss":"sd");return;}
        if(to==ULONG || to==ULLONG) {
            int small=target_label(),end=target_label();
            printf(" subq $8,%%rsp\n movl $1593835520,(%%rsp)\n flds (%%rsp)\n addq $8,%%rsp\n fxch %%st(1)\n fucomi %%st(1),%%st\n jb .L%d\n fsubp\n",small);
            extended_truncate();printf(" movq $-9223372036854775808,%%rcx\n xorq %%rcx,%%rax\n jmp .L%d\n",end);
            deflab(small);puts(" fstp %st(1)");extended_truncate();deflab(end);
        } else extended_truncate();
        normalize(to);return;
    }
    if(floating(from) && floating(to)) {puts(to==FLOAT?" cvtsd2ss %xmm0,%xmm0":" cvtss2sd %xmm0,%xmm0");return;}
    if(floating(to)) {
        if(from==ULONG || from==ULLONG) {
            int ordinary=target_label(),end=target_label();
            printf(" testq %%rax,%%rax\n jns .L%d\n movq %%rax,%%rcx\n andl $1,%%eax\n shrq $1,%%rcx\n orq %%rcx,%%rax\n",ordinary);
            printf(" cvtsi2%sq %%rax,%%xmm0\n add%s %%xmm0,%%xmm0\n jmp .L%d\n",to==FLOAT?"ss":"sd",to==FLOAT?"ss":"sd",end);
            deflab(ordinary);printf(" cvtsi2%sq %%rax,%%xmm0\n",to==FLOAT?"ss":"sd");deflab(end);
        } else printf(" cvtsi2%sq %%rax,%%xmm0\n",to==FLOAT?"ss":"sd");
        return;
    }
    if(floating(from)) {
        const char *s=from==FLOAT?"ss":"sd";
        if(to==ULONG || to==ULLONG) {
            int small=target_label(),end=target_label();uint64_t bits=from==FLOAT?UINT64_C(0x5f000000):UINT64_C(0x43e0000000000000);
            printf(" movq $%lu,%%rax\n movq %%rax,%%xmm1\n ucomi%s %%xmm1,%%xmm0\n jb .L%d\n sub%s %%xmm1,%%xmm0\n cvtt%s2siq %%xmm0,%%rax\n movq $-9223372036854775808,%%rcx\n xorq %%rcx,%%rax\n jmp .L%d\n",(unsigned long)bits,s,small,s,s,end);
            deflab(small);printf(" cvtt%s2siq %%xmm0,%%rax\n",s);deflab(end);
        } else printf(" cvtt%s2siq %%xmm0,%%rax\n",s);
        normalize(to);
    }
}
static void truth(TWORD type) {
    if(extended(type)) {puts(" fldz\n fucomip %st(1),%st\n fstp %st(0)\n setne %al\n setp %cl\n orb %cl,%al\n movzbq %al,%rax");return;}
    if(floating(type)) {
        printf(" pxor %%xmm1,%%xmm1\n ucomi%s %%xmm1,%%xmm0\n setne %%al\n setp %%cl\n orb %%cl,%%al\n movzbq %%al,%%rax\n",type==FLOAT?"ss":"sd");
    }
}
static void float_binary(unsigned op,TWORD type) {
    const char *s=type==FLOAT?"ss":"sd",*instruction=NULL,*condition=NULL;
    if(extended(type)) {
        /* ST0 is the left operand and ST1 the right; popping forms leave the result. */
        static const char *x87[]={"faddp","fsubp","fmulp","fdivp"};
        int arithmetic=op==PLUS?0:op==MINUS?1:op==MUL?2:op==DIV?3:-1;
        if(arithmetic>=0) {printf(" %s\n",x87[arithmetic]);return;}
        puts(" fucomip %st(1),%st\n fstp %st(0)");s=NULL;
    }
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
        if(s)printf(" ucomi%s %%xmm1,%%xmm0\n",s);
        printf(" set%s %%al\n",condition);
        if(op==NE)puts(" setp %cl\n orb %cl,%al");
        else puts(" setnp %cl\n andb %cl,%al");
        puts(" movzbq %al,%rax");
    }
}
static int expand(void *p,char escape,unsigned modifier,const struct mip_node *n) {
    (void)n;struct binary_context *c=p;
    if(!escape)return putchar((int)modifier)==EOF;
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
    TWORD right=node_at(n->right)->type;
    if(floating(left->type) || floating(right) || extended(left->type) || extended(right)) {
        if(n->op==ASSIGN) {generate(n->right,nesting+1);scalar_convert(right,left->type);}
        else {
            TWORD arithmetic=pcc_arithmetic(left->type,right);
            load(left->type);if(post)keep_value(left->type);scalar_convert(left->type,arithmetic);spill_value(arithmetic);
            generate(n->right,nesting+1);scalar_convert(right,arithmetic);
            if(!extended(arithmetic))puts(" movq %xmm0,%xmm1");restore_value(arithmetic);
            float_binary(n->op==INCR?PLUS:n->op==DECR?MINUS:n->op-1,arithmetic);scalar_convert(arithmetic,left->type);
        }
        if(post) {printf(" movq %u(%%rsp),%%rcx\n",spill_bytes(left->type));store(n->left);discard(left->type);restore_value(left->type);pop("r10");}
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
    if(n->op!=STARG && (n->type==STRTY || n->type==UNIONTY))cerror("aggregate argument without structure argument node");
    items[(*count)++]=root;
}
static unsigned class_count(unsigned c,unsigned class) {return (PCC_CLASS_AT(c,0)==class)+(PCC_CLASS_AT(c,1)==class);}
static int in_registers(unsigned c) {unsigned a=PCC_CLASS_AT(c,0);return a==PCC_CLASS_INTEGER || a==PCC_CLASS_SSE;}
static void copy_bytes(const char *destination,uint64_t bytes) {
    printf(" movq %%rax,%%rsi\n leaq %s,%%rdi\n movq $%lu,%%rcx\n cld\n rep movsb\n",destination,(unsigned long)bytes);
}
/* Arguments are staged in one aligned block: memory arguments at the bottom
 * (their final ABI location), register values above them, then the callee. */
static void call(const struct mip_node *n,unsigned nesting) {
    static const char *regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
    uint32_t items[256];unsigned count=0,i,j,ngp=0,nsse=0,offset[256],memory[256],gp[256][2],sse[256][2];
    uint64_t stack=0,stage,total;int structure=n->op==STCALL || n->op==UNARY STCALL;
    unsigned result=structure?n->reg:PCC_CLASS_NONE;char where[64];
    if(n->op==CALL || n->op==STCALL)arguments(n->right,items,&count,nesting+1);
    if(structure && PCC_CLASS_AT(result,0)==PCC_CLASS_MEMORY)ngp=1;
    for(i=0;i<count;i++) {
        const struct mip_node *a=node_at(items[i]);gp[i][0]=gp[i][1]=sse[i][0]=sse[i][1]=UINT32_MAX;memory[i]=0;
        if(a->op==STARG) {
            unsigned c=a->reg,needgp=class_count(c,PCC_CLASS_INTEGER),needsse=class_count(c,PCC_CLASS_SSE);
            if(in_registers(c) && ngp+needgp<=6 && nsse+needsse<=8) {
                for(j=0;j<2;j++) {
                    if(PCC_CLASS_AT(c,j)==PCC_CLASS_INTEGER)gp[i][j]=ngp++;
                    else if(PCC_CLASS_AT(c,j)==PCC_CLASS_SSE)sse[i][j]=nsse++;
                }
            } else {
                if(a->aggregate_alignment>8)stack=(stack+15)&~UINT64_C(15);
                memory[i]=1;offset[i]=(unsigned)stack;stack+=(a->aggregate_size+7)&~UINT64_C(7);
            }
        } else if(extended(a->type)) {stack=(stack+15)&~UINT64_C(15);memory[i]=1;offset[i]=(unsigned)stack;stack+=16;}
        else if(floating(a->type) && nsse<8)sse[i][0]=nsse++;
        else if(!floating(a->type) && ngp<6)gp[i][0]=ngp++;
        else {memory[i]=1;offset[i]=(unsigned)stack;stack+=8;}
        if(stack>0x10000000)cerror("call arguments too large");
    }
    stage=stack;
    for(i=0;i<count;i++)if(!memory[i]) {offset[i]=(unsigned)stage;stage+=node_at(items[i])->op==STARG?16:8;}
    total=stage+8;if((depth*8+total)%16)total+=8;
    printf(" subq $%lu,%%rsp\n",(unsigned long)total);depth+=(unsigned)(total/8);
    generate(n->left,nesting+1);printf(" movq %%rax,%lu(%%rsp)\n",(unsigned long)stage);
    for(i=0;i<count;i++) {
        const struct mip_node *a=node_at(items[i]);generate(items[i],nesting+1);
        if(a->op==STARG) {snprintf(where,sizeof(where),"%u(%%rsp)",offset[i]);copy_bytes(where,a->aggregate_size);}
        else if(extended(a->type))printf(" fstpt %u(%%rsp)\n",offset[i]);
        else printf(" movq %%%s,%u(%%rsp)\n",floating(a->type)?"xmm0":"rax",offset[i]);
    }
    for(i=0;i<count;i++)if(!memory[i])for(j=0;j<2;j++) {
        if(gp[i][j]!=UINT32_MAX)printf(" movq %u(%%rsp),%%%s\n",offset[i]+j*8,regs[gp[i][j]]);
        if(sse[i][j]!=UINT32_MAX)printf(" movq %u(%%rsp),%%xmm%u\n",offset[i]+j*8,sse[i][j]);
    }
    if(structure && PCC_CLASS_AT(result,0)==PCC_CLASS_MEMORY)printf(" leaq %ld(%%rbp),%%rdi\n",(long)n->offset);
    printf(" movq %lu(%%rsp),%%r11\n movl $%u,%%eax\n call *%%r11\n addq $%lu,%%rsp\n",(unsigned long)stage,nsse,(unsigned long)total);
    depth-=(unsigned)(total/8);
    if(structure) {
        unsigned a=PCC_CLASS_AT(result,0),rgp=0,rsse=0;static const char *returned[]={"rax","rdx"};
        if(a==PCC_CLASS_X87)printf(" fstpt %ld(%%rbp)\n",(long)n->offset);
        else if(a!=PCC_CLASS_MEMORY)for(j=0;j<2;j++) {
            if(PCC_CLASS_AT(result,j)==PCC_CLASS_INTEGER)printf(" movq %%%s,%ld(%%rbp)\n",returned[rgp++],(long)n->offset+j*8);
            else if(PCC_CLASS_AT(result,j)==PCC_CLASS_SSE)printf(" movsd %%xmm%u,%ld(%%rbp)\n",rsse++,(long)n->offset+j*8);
        }
        printf(" leaq %ld(%%rbp),%%rax\n",(long)n->offset);
    }
}
/* SysV va_list: gp_offset, fp_offset, overflow_arg_area, reg_save_area. */
static void va_overflow(unsigned alignment,uint64_t bytes) {
    puts(" movq 8(%r10),%rax");
    if(alignment>8)puts(" addq $15,%rax\n andq $-16,%rax");
    printf(" leaq %lu(%%rax),%%rcx\n movq %%rcx,8(%%r10)\n",(unsigned long)((bytes+7)&~UINT64_C(7)));
}
static void va_argument(const struct mip_node *n,unsigned nesting) {
    generate(n->left,nesting+1);puts(" movq %rax,%r10");
    int over=target_label(),done=target_label();
    if(n->aggregate_size) {
        unsigned c=n->reg,needgp=class_count(c,PCC_CLASS_INTEGER),needsse=class_count(c,PCC_CLASS_SSE),j;
        if(in_registers(c)) {
            if(needgp)printf(" movl (%%r10),%%ecx\n cmpl $%u,%%ecx\n ja .L%d\n",48-8*needgp,over);
            if(needsse)printf(" movl 4(%%r10),%%ecx\n cmpl $%u,%%ecx\n ja .L%d\n",176-16*needsse,over);
            for(j=0;j<2;j++) {
                unsigned class=PCC_CLASS_AT(c,j);if(class==PCC_CLASS_NONE)continue;
                unsigned field=class==PCC_CLASS_SSE?4:0;
                printf(" movl %u(%%r10),%%ecx\n movq 16(%%r10),%%rax\n movq (%%rax,%%rcx),%%rax\n movq %%rax,%ld(%%rbp)\n addl $%u,%%ecx\n movl %%ecx,%u(%%r10)\n",
                    field,(long)n->offset+j*8,class==PCC_CLASS_SSE?16:8,field);
            }
            printf(" leaq %ld(%%rbp),%%rax\n jmp .L%d\n",(long)n->offset,done);
        }
        deflab(over);va_overflow(n->aggregate_alignment,n->aggregate_size);deflab(done);return;
    }
    if(extended(n->type)) {deflab(over);va_overflow(16,16);deflab(done);load(n->type);return;}
    unsigned field=floating(n->type)?4:0,limit=floating(n->type)?176:48,step=floating(n->type)?16:8;
    printf(" movl %u(%%r10),%%ecx\n cmpl $%u,%%ecx\n jae .L%d\n movq 16(%%r10),%%rax\n addq %%rcx,%%rax\n addl $%u,%%ecx\n movl %%ecx,%u(%%r10)\n jmp .L%d\n",field,limit,over,step,field,done);
    deflab(over);va_overflow(8,8);deflab(done);load(n->type);
}
static void va_operation(const struct mip_node *n,unsigned nesting) {
    if(n->op==VA_END) {generate(n->left,nesting+1);return;}
    if(n->op==VA_COPY) {
        generate(n->left,nesting+1);push("rax");generate(n->right,nesting+1);puts(" movq %rax,%rsi");pop("rdi");
        puts(" movq (%rsi),%rcx\n movq %rcx,(%rdi)\n movq 8(%rsi),%rcx\n movq %rcx,8(%rdi)\n movq 16(%rsi),%rcx\n movq %rcx,16(%rdi)");return;
    }
    generate(n->left,nesting+1);
    printf(" movl $%u,(%%rax)\n movl $%u,4(%%rax)\n leaq %u(%%rbp),%%rcx\n movq %%rcx,8(%%rax)\n leaq %ld(%%rbp),%%rcx\n movq %%rcx,16(%%rax)\n",
        (unsigned)(n->value&0xffff),(unsigned)(n->value>>16&0xffff),(unsigned)(n->value>>32),(long)n->offset);
}
static void generate(uint32_t root,unsigned nesting) {
    if(nesting>TREESZ)cerror("target expression too deep");
    const struct mip_node *n=node_at(root);unsigned op=n->op;unsigned initial=depth;
    switch(op) {
    case CALL:case UNARY CALL:case STCALL:case UNARY STCALL:call(n,nesting+1);break;
    case STARG:generate(n->left,nesting+1);break;
    case VA_ARG:va_argument(n,nesting+1);break;
    case VA_START:case VA_END:case VA_COPY:va_operation(n,nesting+1);break;
    case STASG:
        address(n->left,nesting+1);push("rax");generate(n->right,nesting+1);
        puts(" movq %rax,%rsi");pop("rdi");
        printf(" movq %%rdi,%%r10\n movq $%lu,%%rcx\n cld\n rep movsb\n movq %%r10,%%rax\n",(unsigned long)n->aggregate_size);break;
    case FP_NAN:case FP_FINITE:case FP_SIGN: {
        TWORD type=node_at(n->left)->type;generate(n->left,nesting+1);
        if(extended(type)) {
            if(op==FP_NAN)puts(" fucomip %st(0),%st\n setp %al\n movzbq %al,%rax");
            else {
                puts(" subq $16,%rsp\n fstpt (%rsp)\n movzwq 8(%rsp),%rax\n addq $16,%rsp");
                if(op==FP_SIGN)puts(" shrq $15,%rax\n andl $1,%eax");
                else puts(" andl $32767,%eax\n cmpl $32767,%eax\n setne %al\n movzbq %al,%rax");
            }
            break;
        }
        if(op==FP_NAN)printf(" ucomi%s %%xmm0,%%xmm0\n setp %%al\n movzbq %%al,%%rax\n",type==FLOAT?"ss":"sd");
        else {
            puts(" movq %xmm0,%rax");
            if(op==FP_SIGN)printf(" shrq $%u,%%rax\n andl $1,%%eax\n",type==FLOAT?31:63);
            else printf(" shrq $%u,%%rax\n andl $%u,%%eax\n cmpl $%u,%%eax\n setne %%al\n movzbq %%al,%%rax\n",type==FLOAT?23:52,type==FLOAT?255:2047,type==FLOAT?255:2047);
        }
        break;
    }
    case FCON: {
        uint64_t bits;
        if(extended(n->type)) {
            uint16_t high;memcpy(&bits,n->extended,8);memcpy(&high,n->extended+8,2);
            printf(" subq $16,%%rsp\n movq $%lu,%%rax\n movq %%rax,(%%rsp)\n movw $%u,8(%%rsp)\n fldt (%%rsp)\n addq $16,%%rsp\n",(unsigned long)bits,(unsigned)high);
            break;
        }
        if(n->type==FLOAT) {float f=(float)n->floating;uint32_t v;memcpy(&v,&f,4);bits=v;}
        else memcpy(&bits,&n->floating,8);
        printf(" movq $%lu,%%rax\n movq %%rax,%%xmm0\n",(unsigned long)bits);break;
    }
    case ICON:
        if(n->name[0])address(root,nesting+1);else printf(" movq $%ld,%%rax\n",(long)n->value);
        normalize(n->type);break;
    case NAME:
        address(root,nesting+1);if(!ISARY(n->type) && !ISFTN(n->type) && !aggregate(n->type))load(n->type);break;
    case UNARY AND:address(n->left,nesting+1);break;
    case UNARY MUL:address(root,nesting+1);if(!aggregate(n->type))load(n->type);break;
    case FLD:
        address(root,nesting+1);load_left(root);break;
    case PCONV:case SCONV:case FORCE:generate(n->left,nesting+1);scalar_convert(node_at(n->left)->type,n->type);normalize(n->type);break;
    case COMOP:case CM:generate(n->left,nesting+1);discard(node_at(n->left)->type);generate(n->right,nesting+1);break;
    case UNARY MINUS:case COMPL:case NOT:
        generate(n->left,nesting+1);
        if(op==NOT) {truth(node_at(n->left)->type);puts(" testq %rax,%rax\n sete %al\n movzbq %al,%rax");}
        else if(extended(n->type) && op==UNARY MINUS)puts(" fchs");
        else if(floating(n->type) && op==UNARY MINUS) {
            printf(" movq $%lu,%%rax\n movq %%rax,%%xmm1\n pxor %%xmm1,%%xmm0\n",n->type==FLOAT?UINT64_C(0x80000000):UINT64_C(0x8000000000000000));
        }
        else {puts(op==COMPL?" notq %rax":" negq %rax");normalize(n->type);}break;
    case ANDAND:case OROR: {
        int shortcut=target_label(),end=target_label();generate(n->left,nesting+1);truth(node_at(n->left)->type);
        printf(" testq %%rax,%%rax\n j%s .L%d\n",op==ANDAND?"z":"nz",shortcut);
        generate(n->right,nesting+1);truth(node_at(n->right)->type);printf(" testq %%rax,%%rax\n setne %%al\n movzbq %%al,%%rax\n jmp .L%d\n",end);
        deflab(shortcut);printf(" movq $%u,%%rax\n",op==OROR);deflab(end);break;
    }
    case QUEST: {
        const struct mip_node *arms=node_at(n->right);if(arms->op!=COLON)cerror("bad conditional tree");
        int alternate=target_label(),end=target_label();generate(n->left,nesting+1);truth(node_at(n->left)->type);printf(" testq %%rax,%%rax\n jz .L%d\n",alternate);
        generate(arms->left,nesting+1);printf(" jmp .L%d\n",end);deflab(alternate);generate(arms->right,nesting+1);deflab(end);break;
    }
    case CBRANCH:
        generate(n->left,nesting+1);truth(node_at(n->left)->type);printf(" testq %%rax,%%rax\n jz .L%lu\n",(unsigned long)node_at(n->right)->value);break;
    case GOTO:generate(n->left,nesting+1);puts(" jmp *%rax");break;
    case ASSIGN:case INCR:case DECR:case ASG PLUS:case ASG MINUS:case ASG MUL:case ASG DIV:case ASG MOD:
    case ASG AND:case ASG OR:case ASG ER:case ASG LS:case ASG RS:assignment(root,nesting+1);break;
    case PLUS:case MINUS:case MUL:case DIV:case MOD:case AND:case OR:case ER:case LS:case RS:case ARS:
    case EQ:case NE:case LT:case LE:case GT:case GE:case ULT:case ULE:case UGT:case UGE:
        if(floating(node_at(n->left)->type) || extended(node_at(n->left)->type)) {
            TWORD type=node_at(n->left)->type;
            generate(n->left,nesting+1);spill_value(type);generate(n->right,nesting+1);
            if(!extended(type))puts(" movq %xmm0,%xmm1");restore_value(type);float_binary(op,type);break;
        }
        generate(n->left,nesting+1);push("rax");generate(n->right,nesting+1);puts(" movq %rax,%rcx");pop("rax");
        binary(root,op,node_at(n->left)->type);normalize(n->type);break;
    default:cerror("operator %u requires remaining backend work",op);
    }
    if(op!=CBRANCH && op!=GOTO && !ISARY(n->type) && !ISFTN(n->type))normalize(n->type);
    if(depth!=initial)cerror("unbalanced target expression stack");
}
void pcc_emit_expression(const struct mip_arena *arena,uint32_t root) {
    current_ir=arena;depth=0;generate(root,0);
    /* A returned long double stays in ST0; any other unused one is popped. */
    if(node_at(root)->op!=FORCE)discard(node_at(root)->type);
    current_ir=NULL;
}
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

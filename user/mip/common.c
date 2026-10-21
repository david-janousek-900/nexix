/* Native adaptation of V7 mip/manifest and common operator/tree services. */
#include "mip.h"

static const struct dopest { int op; char name[12]; int flags; } operators[] = {

    { NAME, "NAME", LTYPE },
    { STRING, "STRING", LTYPE },
    { REG, "REG", LTYPE },
    { OREG, "OREG", LTYPE },
    { ICON, "ICON", LTYPE },
    { FCON, "FCON", LTYPE },
    { CCODES, "CCODES", LTYPE },
    { UNARY MINUS, "U-", UTYPE },
    { UNARY MUL, "U*", UTYPE },
    { UNARY AND, "U&", UTYPE },
    { UNARY CALL, "UCALL", UTYPE|CALLFLG },
    { UNARY FORTCALL, "UFCALL", UTYPE|CALLFLG },
    { NOT, "!", UTYPE|LOGFLG },
    { COMPL, "~", UTYPE },
    { FORCE, "FORCE", UTYPE },
    { INIT, "INIT", UTYPE },
    { SCONV, "SCONV", UTYPE },
    { PCONV, "PCONV", UTYPE },
    { PLUS, "+", BITYPE|FLOFLG|SIMPFLG|COMMFLG },
    { ASG PLUS, "+=", BITYPE|ASGFLG|ASGOPFLG|FLOFLG|SIMPFLG|COMMFLG },
    { MINUS, "-", BITYPE|FLOFLG|SIMPFLG },
    { ASG MINUS, "-=", BITYPE|FLOFLG|SIMPFLG|ASGFLG|ASGOPFLG },
    { MUL, "*", BITYPE|FLOFLG|MULFLG },
    { ASG MUL, "*=", BITYPE|FLOFLG|MULFLG|ASGFLG|ASGOPFLG },
    { AND, "&", BITYPE|SIMPFLG|COMMFLG },
    { ASG AND, "&=", BITYPE|SIMPFLG|COMMFLG|ASGFLG|ASGOPFLG },
    { QUEST, "?", BITYPE },
    { COLON, ":", BITYPE },
    { ANDAND, "&&", BITYPE|LOGFLG },
    { OROR, "||", BITYPE|LOGFLG },
    { CM, ",", BITYPE },
    { COMOP, ",OP", BITYPE },
    { ASSIGN, "=", BITYPE|ASGFLG },
    { DIV, "/", BITYPE|FLOFLG|MULFLG|DIVFLG },
    { ASG DIV, "/=", BITYPE|FLOFLG|MULFLG|DIVFLG|ASGFLG|ASGOPFLG },
    { MOD, "%", BITYPE|DIVFLG },
    { ASG MOD, "%=", BITYPE|DIVFLG|ASGFLG|ASGOPFLG },
    { LS, "<<", BITYPE|SHFFLG },
    { ASG LS, "<<=", BITYPE|SHFFLG|ASGFLG|ASGOPFLG },
    { RS, ">>", BITYPE|SHFFLG },
    { ASG RS, ">>=", BITYPE|SHFFLG|ASGFLG|ASGOPFLG },
    { OR, "|", BITYPE|COMMFLG|SIMPFLG },
    { ASG OR, "|=", BITYPE|COMMFLG|SIMPFLG|ASGFLG|ASGOPFLG },
    { ER, "^", BITYPE|COMMFLG|SIMPFLG },
    { ASG ER, "^=", BITYPE|COMMFLG|SIMPFLG|ASGFLG|ASGOPFLG },
    { INCR, "++", BITYPE|ASGFLG },
    { DECR, "--", BITYPE|ASGFLG },
    { STREF, "->", BITYPE },
    { CALL, "CALL", BITYPE|CALLFLG },
    { FORTCALL, "FCALL", BITYPE|CALLFLG },
    { EQ, "==", BITYPE|LOGFLG },
    { NE, "!=", BITYPE|LOGFLG },
    { LE, "<=", BITYPE|LOGFLG },
    { LT, "<", BITYPE|LOGFLG },
    { GE, ">", BITYPE|LOGFLG },
    { GT, ">", BITYPE|LOGFLG },
    { UGT, "UGT", BITYPE|LOGFLG },
    { UGE, "UGE", BITYPE|LOGFLG },
    { ULT, "ULT", BITYPE|LOGFLG },
    { ULE, "ULE", BITYPE|LOGFLG },
    { ARS, "A>>", BITYPE },
    { TYPE, "TYPE", LTYPE },
    { LB, "[", BITYPE },
    { CBRANCH, "CBRANCH", BITYPE },
    { FLD, "FLD", UTYPE },
    { PMCONV, "PMCONV", BITYPE },
    { PVCONV, "PVCONV", BITYPE },
    { RETURN, "RETURN", BITYPE|ASGFLG|ASGOPFLG },
    { CAST, "CAST", BITYPE|ASGFLG|ASGOPFLG },
    { GOTO, "GOTO", UTYPE },
    { STASG, "STASG", BITYPE|ASGFLG },
    { STARG, "STARG", UTYPE },
    { STCALL, "STCALL", BITYPE|CALLFLG },
    { UNARY STCALL, "USTCALL", UTYPE|CALLFLG },

    { VA_START, "VA_START", BITYPE },
    { VA_END, "VA_END", UTYPE },
    { VA_COPY, "VA_COPY", BITYPE },
    { VA_ARG, "VA_ARG", UTYPE },
    { FP_NAN, "FP_NAN", UTYPE },
    { FP_FINITE, "FP_FINITE", UTYPE },
    { FP_SIGN, "FP_SIGN", UTYPE },
    { -1, "", 0 }
};

unsigned mip_op_flags(unsigned op) {
    for (unsigned i=0; operators[i].op>=0; ++i)
        if ((unsigned)operators[i].op==op) return operators[i].flags;
    return 0;
}
const char *mip_op_name(unsigned op) {
    for (unsigned i=0; operators[i].op>=0; ++i)
        if ((unsigned)operators[i].op==op) return operators[i].name;
    return 0;
}
int mip_type_valid(mip_type type) {
    /* Five basic-type bits, twenty-nine modifiers, top bit reserved. */
    if((type>>63) || (type&BTMASK)>BOOL)return 0;
    for (type >>= BTSHIFT; type; type >>= TSHIFT)
        if (!(type & 3)) return 0;
    return 1;
}
int mip_type_add(mip_type type, unsigned modifier, mip_type *out) {
    if (!out || !mip_type_valid(type) ||
        (modifier!=PTR && modifier!=FTN && modifier!=ARY) ||
        (type >> 61)) return -1;
    *out = ((type & ~(mip_type)BTMASK)<<TSHIFT) | modifier | (type&BTMASK);
    return 0;
}
int mip_type_remove(mip_type type, mip_type *out) {
    if (!out || !mip_type_valid(type) || !(type & TMASK)) return -1;
    *out = ((type >> TSHIFT) & ~(mip_type)BTMASK) | (type & BTMASK);
    return 0;
}
int mip_scalar_layout(mip_type type, struct mip_layout *out) {
    static const unsigned char sizes[21] = {0,0,1,2,4,8,4,8,0,0,4,0,1,2,4,8,8,8,16,0,1};
    unsigned n;
    if (!out || !mip_type_valid(type)) return -1;
    if ((type & TMASK)==PTR) n=8;
    else if (type & TMASK) return -1;
    else n=sizes[type];
    if (!n) return -1;
    out->size=n; out->alignment=n;
    return 0;
}
int mip_align(uint64_t offset, unsigned alignment, uint64_t *out) {
    if (!out || !alignment || (alignment & (alignment-1)) ||
        offset > UINT64_MAX-(alignment-1)) return -1;
    *out = (offset+alignment-1) & ~(uint64_t)(alignment-1);
    return 0;
}
void mip_arena_init(struct mip_arena *a, struct mip_node *nodes, uint32_t capacity) {
    a->nodes=nodes; a->count=0; a->capacity=nodes ? capacity : 0;
}
int mip_node_valid(const struct mip_arena *a, const struct mip_node *n) {
    unsigned arity;
    size_t name_length;
    if (!a || !n || !a->nodes || a->count>=a->capacity ||
        !mip_type_valid(n->type)) return -1;
    for (name_length=0; name_length<sizeof(n->name) && n->name[name_length]; ++name_length) {}
    if (name_length==sizeof(n->name)) return -1;
    arity=mip_op_flags(n->op)&TYFLG;
    if (!arity || (arity==LTYPE && (n->left!=MIP_NO_NODE || n->right!=MIP_NO_NODE)) ||
        (arity==UTYPE && (n->left>=a->count || n->right!=MIP_NO_NODE)) ||
        (arity==BITYPE && (n->left>=a->count || n->right>=a->count))) return -1;
    if (n->op==STASG || n->op==STARG || n->op==STCALL || n->op==UNARY STCALL) {
        uint64_t size;
        if (mip_align(n->aggregate_size,n->aggregate_alignment,&size) ||
            size!=n->aggregate_size) return -1;
    }
    if (n->op==REG && n->reg>=64) return -1;
    if (n->op==FLD && (!n->field_size || n->field_size>64 || n->field_shift>64-n->field_size)) return -1;
    return 0;
}
int mip_node_add(struct mip_arena *a,const struct mip_node *n,uint32_t *out) {
    if(!out || mip_node_valid(a,n)) return -1;
    a->nodes[a->count]=*n;
    *out=a->count++;
    return 0;
}

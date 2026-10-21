/* Native interfaces for the retained K&R front end. */
NODE *buildtree(int,NODE *,NODE *), *bdty(int,NODE *,int), *mkty(TWORD,int,int), *rstruct(int,int), *dclstruct(int), *getstr(void), *tymerge(NODE *,NODE *), *stref(NODE *), *offcon(OFFSZ,TWORD,int,int), *bcon(int), *bpsize(NODE *), *convert(NODE *,int), *pconvert(NODE *), *oconvert(NODE *), *ptmatch(NODE *), *tymatch(NODE *), *makety(NODE *,TWORD,int,int), *block(int,NODE *,NODE *,TWORD,int,int), *doszof(NODE *), *talloc(void), *optim(NODE *), *strargs(NODE *), *clocal(NODE *), *cast(NODE *,TWORD);
OFFSZ tsize(TWORD,int,int),psize(NODE *);
TWORD types(TWORD,TWORD,TWORD),ctype(TWORD);
char *exname(char *),*exdcon(NODE *);
void uerror(const char *,...),cerror(const char *,...),werror(const char *,...);
int talign(TWORD,int),inforce(OFFSZ),instk(int,TWORD,int,int,OFFSZ);
int andable(NODE *),cisreg(TWORD),cendarg(void),noinit(void),isitfloat(char *);
int mainp1(int,char **);
int ecode(NODE *);

TWORD pcc_promote(TWORD),pcc_arithmetic(TWORD,TWORD);
uint64_t pcc_integer_bits(uint64_t,TWORD);
int pcc_conval(NODE *,int,NODE *),pcc_number(int);

uint32_t pcc_lower(NODE *,struct mip_arena *);

int lookup(char *,int),pcc_lookup_owner(char *,int,int),pcc_member(int,const char *);

int pcc_newline(void);

TWORD pcc_value_type(NODE *);

unsigned pcc_proto_begin(void),pcc_proto_end(void),pcc_proto_empty(void),pcc_proto_result(unsigned);
void pcc_proto_parameter(NODE *,NODE *),pcc_proto_variadic(void),pcc_proto_definition(unsigned);
int pcc_proto_compatible(unsigned,unsigned),pcc_typed_definition(void);
NODE *pcc_call_arguments(NODE *,NODE *);
void pcc_proto_link(unsigned,unsigned);

long double pcc_real(const char *,TWORD *);
int fincode(long double,int);
NODE *pcc_vararg(unsigned,NODE *,NODE *,NODE *);
void pcc_align_type(NODE *,int);
unsigned pcc_proto_merge(unsigned,unsigned);
unsigned pcc_ir_capacity(NODE *);
int pcc_special_real(int);
NODE *pcc_predicate(unsigned,NODE *);
void pcc_check_definition(const int *,int);

/* Packed System V eightbyte classes carried in shared IR reg fields. */
#define PCC_CLASS_NONE 0
#define PCC_CLASS_INTEGER 1
#define PCC_CLASS_SSE 2
#define PCC_CLASS_X87 3
#define PCC_CLASS_MEMORY 4
#define PCC_PACK_CLASS(a,b) ((unsigned)(a)|((unsigned)(b)<<4))
#define PCC_CLASS_AT(c,i) (((c)>>((i)*4))&15U)
unsigned pcc_classify(int);
int pcc_definition_variadic(void);
/* Variadic definition state: register save area and initial va_list offsets. */
struct pcc_va_frame {int save;unsigned gp,fp,overflow;};
extern struct pcc_va_frame pcc_va_frame;
void pcc_note_frame(unsigned);

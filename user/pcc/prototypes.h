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

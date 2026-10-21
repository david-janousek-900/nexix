/* Native shared adaptation of the V7 mip/mfile2 table interface. */
#ifndef NX_MIP_TEMPLATES_H
#define NX_MIP_TEMPLATES_H
#include "mip.h"
/*	cookies, used as arguments to codgen */

# define FOREFF 01 /* compute for effects only */
# define INAREG 02 /* compute into a register */
# define INTAREG 04 /* compute into a scratch register */
# define INBREG 010 /* compute into a lvalue register */
# define INTBREG 020 /* compute into a scratch lvalue register */
# define FORCC 040 /* compute for condition codes only */
# define INTEMP 010000 /* compute into a temporary location */
# define FORARG 020000 /* compute for an argument of a function */
# define FORREW 040000 /* search the table, looking for a rewrite rule */

	/* OP descriptors */
	/* the ASG operator may be used on some of these */

# define OPSIMP 010000    /* +, -, &, |, ^ */
# define OPCOMM 010002  /* +, &, |, ^ */
# define OPMUL 010004  /* *, / */
# define OPDIV 010006 /* /, % */
# define OPUNARY 010010  /* unary ops */
# define OPLEAF 010012  /* leaves */
# define OPANY 010014  /* any op... */
# define OPLOG 010016 /* logical ops */
# define OPFLOAT 010020 /* +, -, *, or / (for floats) */
# define OPSHFT 010022  /* <<, >> */
# define OPLTYPE 010024  /* leaf type nodes (e.g, NAME, ICON, etc. ) */

	/* match returns */

# define MNOPE 010000
# define MDONE 010001

	/* shapes */

# define SANY 01	/* same as FOREFF */
# define SAREG 02	/* same as INAREG */
# define STAREG 04	/* same as INTAREG */
# define SBREG 010	/* same as INBREG */
# define STBREG 020	/* same as INTBREG */
# define SCC 040	/* same as FORCC */
# define SNAME 0100
# define SCON 0200
# define SFLD 0400
# define SOREG 01000
# define STARNM 02000
# define STARREG 04000
# define SWADD 040000
# define SPECIAL 0100000
# define SZERO SPECIAL
# define SONE (SPECIAL|1)
# define SMONE (SPECIAL|2)

	/* FORARG and INTEMP are carefully not conflicting with shapes */

	/* types */

# define TCHAR 01
# define TSHORT 02
# define TINT 04
# define TLONG 010
# define TFLOAT 020
# define TDOUBLE 040
# define TPOINT 0100
# define TUCHAR 0200
# define TUSHORT 0400
# define TUNSIGNED 01000
# define TULONG 02000
# define TPTRTO 04000  /* pointer to one of the above */
# define TANY 010000  /* matches anything within reason */
# define TSTRUCT 020000   /* structure or union */

	/* reclamation cookies */

# define RNULL 0    /* clobber result */
# define RLEFT 01
# define RRIGHT 02
# define RESC1 04
# define RESC2 010
# define RESC3 020
# define RESCC 04000
# define RNOP 010000   /* DANGER: can cause loops.. */

	/* needs */

# define NAREG 01
# define NACOUNT 03
# define NAMASK 017
# define NASL 04  /* share left register */
# define NASR 010 /* share right register */
# define NBREG 020
# define NBCOUNT 060
# define NBMASK 0360
# define NBSL 0100
# define NBSR 0200
# define NTEMP 0400
# define NTMASK 07400
# define REWRITE 010000


# define MUSTDO 010000   /* force register requirements */
# define NOPREF 020000  /* no preference for register assignment */



#define MIP_REG_LIMIT 64
struct mip_register { const char *name; unsigned shapes; uint64_t aliases; };
struct mip_resources {
    const struct mip_register *registers;
    unsigned count, busy[MIP_REG_LIMIT];
    uint64_t temporary, temporary_limit;
};
struct mip_allocation { unsigned count; int regs[3]; uint64_t temporary, size; };
struct mip_template {
    unsigned op, visit, lshape, ltype, rshape, rtype, needs, rewrite;
    const char *text;
};
int mip_resources_init(struct mip_resources *, const struct mip_register *, unsigned, uint64_t temporary_limit);
int mip_register_hold(struct mip_resources *, unsigned);
int mip_register_release(struct mip_resources *, unsigned);
int mip_allocate(struct mip_resources *, unsigned needs, unsigned size, unsigned alignment, struct mip_allocation *);
int mip_type_match(mip_type, unsigned);
int mip_operator_match(unsigned, unsigned);
int mip_shape_match(const struct mip_arena *, uint32_t, unsigned, const struct mip_resources *);
/* First matching table entry with available resources; -1 invalid, 0 none, 1 match.
 * On no match or invalid input resource state/output arguments are unchanged. */
int mip_match(const struct mip_arena *, uint32_t, unsigned cookie,
              const struct mip_template *, size_t, struct mip_resources *,
              size_t *selected, struct mip_allocation *);
/* Table bytecode retains A/C/I/U/B + L/R/1/2/3, O/Z target escapes,
 * F (effect-only suppression), T, S/H/M/N field data and L labels.
 * Callback receives an escape, modifier and the selected node. It appends to
 * an owned output stream and returns nonzero on failure. Literal text uses
 * escape=0 and modifier=unsigned character. No unchecked printf formats. */
typedef int (*mip_expand_callback)(void *, char, unsigned, const struct mip_node *);
int mip_expand(const struct mip_arena *, uint32_t, unsigned cookie,
               const char *, const struct mip_node results[3],
               mip_expand_callback, void *);
#endif

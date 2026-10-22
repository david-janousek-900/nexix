/*
 * Native c0 -> c1 intermediate file contract.
 *
 * The supplied passes exchanged 16-bit PDP-11 words; Nexix uses 64-bit
 * little-endian words so constants, offsets and types keep native widths.
 * An operator word carries CP_MARK in its upper bits, as the original
 * 0376 marker byte did.  Names and floating literals are NUL-terminated
 * byte strings.
 */
#ifndef NX_C_PASS_H
#define NX_C_PASS_H
#define	CP_MARK		0x7e000000L
#define	CP_MARKMASK	(~0xffL)
#define	CP_VERSION	1
/* Native additions to the intermediate operators (supplied ones are < 223). */
#define	CP_PARAM	223	/* class, register, home offset, type */
#define	CP_VASTART	224	/* va_start(ap): save area, gp, fp, overflow offsets */
#define	CP_VAARG	225
#define	CP_VAEND	226
#define	CP_VACOPY	227
#define	CP_STRARG	228	/* structure argument: size, class word */
#define	CP_SFUNC	229	/* structure-returning function: size, class, hidden slot, temporary */
#define	CP_FILE		230	/* source file name for diagnostics */
#define	CP_VASAVE	231	/* save area offset, gp, fp, overflow offsets */
#define	CP_STRCALL	232	/* structure-valued call: size, class word, frame temporary */
#define	CP_SINGLE	233	/* pass a double argument as a float */
/* System V eightbyte classes packed four bits each in a class word. */
#define	CP_CNONE	0
#define	CP_CINT		1
#define	CP_CSSE		2
#define	CP_CMEM		4
#define	CP_CLASS(c,i)	(((c)>>((i)*4))&15)
/* Parameter register classes. */
#define	CP_PGP		1
#define	CP_PSSE		2
#endif

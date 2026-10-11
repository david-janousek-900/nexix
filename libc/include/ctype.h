#ifndef NEXIX_LIBC_CTYPE_H
#define NEXIX_LIBC_CTYPE_H
/* Retained V7 classification bits and ASCII table contract, extended to 256
 * byte values so native callers can safely classify unsigned char and EOF. */
#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
extern char _ctype_[257];
static inline int nx_ctype(int c) { return c >= -1 && c <= 255 ? _ctype_[c+1] : 0; }
#define isalpha(c) (nx_ctype(c)&(_U|_L))
#define isupper(c) (nx_ctype(c)&_U)
#define islower(c) (nx_ctype(c)&_L)
#define isdigit(c) (nx_ctype(c)&_N)
#define isxdigit(c) (nx_ctype(c)&(_N|_X))
#define isspace(c) (nx_ctype(c)&_S)
#define ispunct(c) (nx_ctype(c)&_P)
#define isalnum(c) (nx_ctype(c)&(_U|_L|_N))
#define isprint(c) (nx_ctype(c)&(_P|_U|_L|_N))
#define iscntrl(c) (nx_ctype(c)&_C)
#define isascii(c) ((unsigned)(c)<=0177)
#define toascii(c) ((c)&0177)
/* V7 macros are arithmetic conversions: callers test islower/isupper first. */
#define toupper(c) ((c)-'a'+'A')
#define tolower(c) ((c)-'A'+'a')
#endif

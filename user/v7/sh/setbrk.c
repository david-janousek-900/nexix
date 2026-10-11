#
/*
 *	UNIX shell
 *
 *	S. R. Bourne
 *	Bell Telephone Laboratories
 *
 */

#include	"defs.h"

BYTPTR setbrk(incr)
intptr_t incr;
{
	REG BYTPTR	a=sbrk(incr);
	if (a==(void *)-1) error(nospace);
	brkend=a+incr;
	return(a);
}

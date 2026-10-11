/* Native adaptation of reference/v7/usr/src/libm/pow.c. See reference/Caldera-license.pdf. */
#include <math.h>
/*
	computes a^b.
	uses log and exp
*/

#include	<errno.h>

double log(), exp();

double
pow(arg1,arg2)
double arg1, arg2;
{
	double temp;
	double integer, half;

	if(arg1 <= 0.) {
		if(arg1 == 0.) {
			if(arg2 <= 0.)
				goto domain;
			return(0.);
		}
		if(!isfinite(arg2) || modf(arg2, &integer) != 0.)
			goto domain;
		temp = exp(arg2 * log(-arg1));
		if(modf(integer / 2., &half) != 0.)
			temp = -temp;
		return(temp);
	}
	return(exp(arg2 * log(arg1)));

domain:
	errno = EDOM;
	return(0.);
}

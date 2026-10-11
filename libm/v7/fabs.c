/* Native adaptation of reference/v7/usr/src/libm/fabs.c. See reference/Caldera-license.pdf. */
#include <math.h>
double
fabs(arg)
double arg;
{

	if(signbit(arg))
		arg = -arg;
	return(arg);
}

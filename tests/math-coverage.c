#include <math.h>
#include <errno.h>
#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX-1)
#include <stdio.h>
#define CHECK(x) do{if(!(x)){printf("math failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static int near(double a,double b,double tolerance){return fabs(a-b)<=tolerance;}
int main(void)
{
    CHECK(near(gamma(1),0,1e-14) && signgam==0);
    CHECK(near(gamma(2),0,1e-14));
    CHECK(near(gamma(.5),.572364942924700087,1e-13));
    CHECK(near(gamma(5),3.17805383034794562,1e-13));
    CHECK(near(gamma(10),12.8018274800814696,1e-12));
    CHECK(near(gamma(-.5),1.2655121234846454,1e-12) && signgam==1);
    CHECK(near(gamma(-1.5),.860047015376481,1e-12) && signgam==0);
    CHECK(near(gamma(0x1p-1074),744.4400719213812,1e-11));
    CHECK(isfinite(gamma(-1+0x1p-40)) && signgam==1);
    errno=0;CHECK(gamma(-2)==HUGE && errno==EDOM);
    errno=0;CHECK(gamma(0)==HUGE && errno==EDOM);
    errno=0;CHECK(gamma(1e300)==HUGE && errno==ERANGE);
    CHECK(isnan(gamma(NAN)));
    CHECK(near(sin(1),.8414709848078965,1e-14));
    CHECK(near(cos(1),.5403023058681397,1e-14));
    CHECK(isfinite(sin(1e300)) && fabs(sin(1e300))<=1.0);
    CHECK(isfinite(cos(1e300)) && fabs(cos(1e300))<=1.0);
    CHECK(isfinite(tan(1e300)));
    errno=0;CHECK(isnan(sin(INFINITY)) && errno==EDOM);
    errno=0;CHECK(isnan(tan(INFINITY)) && errno==EDOM);
    CHECK(hypot(INFINITY,INFINITY)==INFINITY && hypot(INFINITY,NAN)==INFINITY);
    CHECK(isnan(hypot(NAN,1)));
    CHECK(jn(INT_MIN,0)==0 && jn(INT_MAX,0)==0);
    CHECK(near(jn(-1,1),-j1(1),1e-14));
    CHECK(near(yn(-1,1),-y1(1),1e-14));
    errno=0;CHECK(isnan(jn(INT_MAX,INFINITY)) && errno==EDOM);
    errno=0;CHECK(isnan(yn(INT_MIN,INFINITY)) && errno==EDOM);
    errno=0;CHECK(log(0)==-HUGE && errno==EDOM);
    errno=0;CHECK(sqrt(-1)==0 && errno==EDOM);
    errno=0;CHECK(pow(-2,.5)==0 && errno==EDOM);
    CHECK(near(pow(-2,3),-8,1e-13) && near(pow(-2,4),16,1e-13));
    puts("MATH_COVERAGE_OK");return 0;
}

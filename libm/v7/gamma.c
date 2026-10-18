/* Native C equivalent of historical V6 gamma.s, absent from pinned V7 libm.
 * Coefficients and algorithm: reference/supplemental/v6/gamma.s.
 * Historical signgam is 0 for positive Gamma and 1 for negative Gamma.
 * See reference/Caldera-license.pdf and docs/math-coverage.md. */
#include <math.h>
#include <errno.h>
int signgam;
static double positive(double x)
{
    double result;
    if(x>8.0){
        double inverse=1.0/x,t=inverse*inverse;
        double correction=(((((-.001633436431*t+.00083645878922)*t
            -.0005951896861197)*t+.000793650576493454)*t
            -.002777777777735865004)*t+.083333333333333101837)*inverse;
        result=(x-.5)*log(x)-x+.91893853320467274178+correction;
    }else{
        double correction=0;
        while(x<2.0){correction-=log(x);x+=1.0;}
        while(x>3.0){x-=1.0;correction+=log(x);}
        double t=x-2.0;
        double p=(((((( -6.7449507245925289918*t-50.108693752970953015)*t
            -439.33044406002567613)*t-2008.5274013072791214)*t
            -8762.7102978521489560)*t-20886.861789269887364)*t
            -42353.689509744089647);
        double q=(((((((t-23.081551524580124562)*t+189.49823415702801641)*t
            -499.02852662143904834)*t-1528.6072737795220248)*t
            +9940.3074150827709015)*t-2980.3853309256649932)*t
            -42353.689509744090010);
        result=log(p/q)+correction;
    }
    if(!isfinite(result) || result>HUGE){errno=ERANGE;return HUGE;}
    return result;
}
double gamma(double x)
{
    const double pi=3.1415926535897932384626433832795;
    signgam=0;
    if(isnan(x))return x;
    if(x==INFINITY){errno=ERANGE;return HUGE;}
    if(x>0.0)return positive(x);
    if(!isfinite(x) || floor(x)==x){errno=EDOM;return HUGE;}
    /* Reduce before multiplying by pi, retaining accuracy near negative poles. */
    double reduced=fmod(x,2.0);
    if(reduced<-1.0)reduced+=2.0;
    if(reduced>.5)reduced=1.0-reduced;
    if(reduced<-.5)reduced=-1.0-reduced;
    double sine=sin(pi*reduced);
    signgam=sine<0.0;
    return log(pi)-log(-x)-log(fabs(sine))-positive(-x);
}

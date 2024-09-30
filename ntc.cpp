
#include "ntc.h"
#include <cmath>

using namespace std;

/*
 * The B parameter below is taken for a Murata NCP18WF104E03RB which states
 * B=4250 for a 25..50°C range. We "assume" that the result is symmetric also
 * in the 0..25°C range and we also extend the range by 10°C both ways
 */
static const float tMin=-10.f;
static const float tMax=60.f;

NtcResult computeTemperature(unsigned short adc)
{
    /*
    kill(all);
    x:log(r/r0)/(1/t-1/t0)=B;
    z:solve(subst([t0=25+273.15,r0=100000,B=4250],x),r);
    rh:rhs(z[1]);
    rl:100000;
    eq:adc=rl/(rh+rl);
    solve(eq,t);
     */
    if(adc==0) return NtcResult(); //This would result in a division by 0
    float a=adc/65536.f; //Map ADC range in [0..1)
    float t=25342750.f/(5963.f*logf(1.f/a-1.f)+85000.f)-273.15f;
    if(t<tMin) return NtcResult(t,NtcResult::T_UNDERFLOW);
    if(t>tMax) return NtcResult(t,NtcResult::T_OVERFLOW);
    return NtcResult(t,NtcResult::OK);
}

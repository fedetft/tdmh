
#pragma once

#include <algorithm>
#include <cmath>

/**
 * Proportional-Integral (PI) controller with antiwindup
 */
class TemperatureController
{
public:
    /**
     * Constructor
     * \param K controller gain
     * \param T controller pole
     * \param Ts sample time, period in seconds at which the run() method is called
     */
    TemperatureController(float T = 200.0f, float Ts = 1.0f) : T(T), Ts(Ts) {}

    /**
     * Run one step of the controller. To be called periodically every Ts seconds
     * \param pv measured temperature
     * \param sp set point temperature
     * \return actuator power in [0..1] range
     */
    float step(float pv, float sp) {
        // r:k*(1+s*T)/s;
        // ratsimp(subst(s=(z-1)/z/Ts,r));
        if(pv == -1) { // sensor error
            u1 = u2 = e3 = 0.f;
        } else {

            float e2 = sp - pv;

            if (skipped) {
                skipped = false;
                u2 = u1;
                e3 = e2;
            }

            float u = a*u1 + (1-a)*u2 + (((1-a)*(Ts+T))/(mu*Ts))*e2 - (((1-a)*T)/(mu*Ts))*e3;

            e3 = e2;

            u2 = u1;
            u1 = max(0.f, min(1.f, u));
        }

        return u1;
    }

    void skipStep() {
        skipped = true;
    }

private:
    bool skipped = false;
    const float a = 0.9985;
    const float mu = 1250;
    const float T, Ts;
    float u1=0.f, u2=0.f;
    float e3=0.f;
};

/*
NOTE: lower power heater model:   P=1250/(1+400*%s);
      higher power heater model:  P=1250/(1+200*%s);
      using a controller with an "averaged" zero, it would ideally slightly
      overshoot with the low power heater and be slightly slower with the higher
      power one. Of course, the real process has more poles and is nonlinear
      so it's all an approximation after all, but a good one.

t=1:6000;
u=ones(t);
u(1)=0;
P=1250/(1+200*%s);
R=1.4e-6*(1+300*%s)/%s;
L=R*P;
F=L/(1+L);
subplot(211);
plot(t,csim(u,t,syslin('c',P)));
subplot(212);
stepresponse=csim(u,t,syslin('c',F));
plot(t,stepresponse);
printf("Response time to 99%% = %f (minutes)",find(stepresponse>.99)(1)/60);
*/
// class TunedController : public TemperatureController
// {
// public:
//     TunedController() : TemperatureController(1.4e-6f,300.f,1.f) {}
// };

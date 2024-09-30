/*
 * 100K NTC algorithm.
 * Assumes that the driving scheme connects the NTC between VCC and an ADC
 * channel, and that a 100K resistor is connected between the ADC channel and
 * ground.
 */

#ifndef NTC_H
#define NTC_H

class NtcResult
{
public:
    enum Status {
        OK,
        INVALID,
        T_UNDERFLOW,
        T_OVERFLOW
    };

    NtcResult() : temperature(-273.15f), status(INVALID) {}
    NtcResult(float t, Status s) : temperature(t), status(s) {}

    float temperature;
    Status status;
};

/**
 * Compute the humidity based on a 100K NTC reading
 * \param adc the ADC reading, scaled from 0 (GND) to 65535 (VCC)
 * \return the computed temperature. If status is OK the result is valid,
 * otherwise a value is still returned, but clamped to the domain boundary.
 */
NtcResult computeTemperature(unsigned short adc);

#endif //NTC_H

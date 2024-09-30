
#pragma once

/**
 * Efm32 ADC driver
 * 
 * Example code
 * \code
 * {
 *      FastInterruptDisableLock dLock;
 *      //GPIO digital logic must be disabled to use as ADC input
 *      expansion::gpio0::mode(Mode::DISABLED); //GPIO0 is PD3 or ADC0_CH3
 *      expansion::gpio1::mode(Mode::OUTPUT_HIGH);
 *  }
 *  auto& adc=Adc::instance();
 *  adc.powerMode(Adc::OnDemand);
 *  for(;;)
 *  {
 *      iprintf("%d\n",adc.readChannel(3));
 *      Thread::sleep(500);
 *  }
 * \endcode
 */
class Adc
{
public:
    /** 
     * Possible ADC power modes
     */
    enum AdcPowerMode
    {
        Off, On, OnDemand
    };
    
    /**
     * \return an instance to the ADC (singleton)
     */
    static Adc& instance();
    
    /**
     * Change the ADC power mode
     * \param mode power mode
     */
    void powerMode(AdcPowerMode mode);
    
    /**
     * Read an ADC channel
     * \param channel channel to read
     * \return ADC sample, in the 0..4095 range
     */
    unsigned short readChannel(int channel);

public:
    Adc();
};

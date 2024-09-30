
#include "adc.h"
#include "miosix.h"

Adc& Adc::instance()
{
    static Adc singleton;
    return singleton;
}

void Adc::powerMode(AdcPowerMode mode)
{
    ADC0->CTRL &= ~ 0b11; //Clear WARMUPMODE field
    //NOTE: from an implementation perspective Off is the same as OnDemand
    //but from a logical perspective Off is used to turn the ADC off after it
    //has been turned on, while OnDemand is used to pay the startup cost at
    //every conversion
    if(mode==On)
    {
        ADC0->CTRL |= ADC_CTRL_WARMUPMODE_KEEPADCWARM;
        while((ADC0->STATUS & ADC_STATUS_WARM)==0) ;
    }
}

unsigned short Adc::readChannel(int channel)
{
    auto temp = ADC0->SINGLECTRL;
    temp &= ~(0xf<<8); //Clear INPUTSEL field
    temp |= (channel & 0xf)<<8;
    ADC0->SINGLECTRL = temp;
    ADC0->CMD=ADC_CMD_SINGLESTART;
    while((ADC0->STATUS & ADC_STATUS_SINGLEDV)==0) ;
    return ADC0->SINGLEDATA;
}

Adc::Adc()
{
    {
        miosix::FastInterruptDisableLock dLock;
        CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_ADC0;
    }
    ADC0->CTRL = 0b11111<<16             //TIMEBASE ??
               | (4-1)<<8                //PRESC 48/4=12MHz < 13MHz OK
               | ADC_CTRL_LPFMODE_RCFILT
               | ADC_CTRL_WARMUPMODE_NORMAL;
    //TODO: expose more options
    ADC0->SINGLECTRL = ADC_SINGLECTRL_AT_256CYCLES
                     | ADC_SINGLECTRL_REF_VDD
                     | ADC_SINGLECTRL_RES_12BIT
                     | ADC_SINGLECTRL_ADJ_RIGHT;
}

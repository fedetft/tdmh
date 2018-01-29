/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   lasthopassignmentphase.cpp
 * Author: paolo
 * 
 * Created on January 29, 2018, 12:34 AM
 */

#include "lasthopassignmentphase.h"

namespace miosix {
    LastHopAssignmentPhase::~LastHopAssignmentPhase() {
    }
    
    void LastHopAssignmentPhase::execute(MACContext& ctx)
    {
        auto* config = ctx.getTransceiverConfig();
        transceiver.configure(new TransceiverConfiguration(config->frequency, config->txPower, false, false));
        transceiver.turnOn();
        receiveOrGetEmpty(ctx);
        transceiver.turnOff();
        ledOff();
    }
}


/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dynamicassignmentphase.cpp
 * Author: paolo
 * 
 * Created on January 28, 2018, 9:54 PM
 */

#include "dynamicassignmentphase.h"

namespace miosix {
    DynamicAssignmentPhase::~DynamicAssignmentPhase() {
    }
    void DynamicAssignmentPhase::execute(MACContext& ctx)
    {
        auto* config = ctx.getTransceiverConfig();
        transceiver.configure(new TransceiverConfiguration(config->frequency, config->txPower, false, false));
        transceiver.turnOn();
        receiveOrGetEmpty(ctx);
        forwardPacket();
        transceiver.turnOff();
        ledOff();
    }
}


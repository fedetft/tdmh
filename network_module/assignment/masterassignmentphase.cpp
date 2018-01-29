/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   masterassignmentphase.cpp
 * Author: paolo
 * 
 * Created on January 28, 2018, 9:54 PM
 */

#include "masterassignmentphase.h"

namespace miosix {
    MasterAssignmentPhase::~MasterAssignmentPhase() {
    }

    void MasterAssignmentPhase::execute(MACContext& ctx)
    {
        auto* config = ctx.getTransceiverConfig();
        transceiver.configure(new TransceiverConfiguration(config->frequency, config->txPower, false, false));
        transceiver.turnOn();
        getEmptyPkt(ctx.getMediumAccessController().getPanId(), ctx.getHop());
        populatePacket();
        forwardPacket();
        transceiver.turnOff();
        ledOff();
    }
    
    void MasterAssignmentPhase::populatePacket() {
        for (unsigned char e : *assignments){
            packet[7 + (e - 1)/2] |= e & 1? 0xF0 : 0x0F;
        }
    }

}


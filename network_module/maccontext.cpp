/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   maccontext.cpp
 * Author: paolo
 * 
 * Created on December 5, 2017, 6:05 PM
 */

#include "maccontext.h"
#include "macround/macroundfactory.h"
#include "flooding/syncstatus.h"
#include "interfaces-impl/transceiver.h"

namespace miosix {
    
    MACContext::MACContext(const MACRoundFactory* const roundFactory, const miosix::MediumAccessController& mac, bool debug) :
            mac(mac),
            transceiverConfig(new TransceiverConfiguration(mac.getRadioFrequency(), mac.getTxPower(), true, false)),
            currentRound(roundFactory->create(*this, debug)) {}

    MACContext::~MACContext() {
    }

    MACRound* MACContext::shiftRound() {
        currentRound = nextRound;
        nextRound = nullptr;
        return currentRound;
    }
    
    void MACContext::initializeSyncStatus(SyncStatus* syncStatus) { this->syncStatus = syncStatus; }

}

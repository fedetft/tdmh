/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   maccontext.h
 * Author: paolo
 *
 * Created on December 5, 2017, 6:05 PM
 */

#ifndef MACCONTEXT_H
#define MACCONTEXT_H

#include "interfaces-impl/transceiver.h"

namespace miosix {
    class MACRound;
    class MACRoundFactory;
    class MediumAccessController;
    class SyncStatus;
    class MACContext {
    public:
        MACContext() = delete;
        MACContext(const MACContext& orig) = delete;
        MACContext(const MACRoundFactory* const roundFactory, const MediumAccessController& mac);
        virtual ~MACContext();
        MACRound* getCurrentRound() const { return currentRound; }
        MACRound* getNextRound() const { return nextRound; }
        MACRound* shiftRound();
        inline const MediumAccessController& getMediumAccessController() { return mac; }
        void setHop(unsigned char num) { hop = num; }
        unsigned char getHop() { return hop; }
        void setNextRound(MACRound* round);
        void initializeSyncStatus(SyncStatus* syncStatus);
        inline SyncStatus* getSyncStatus() { return syncStatus; }
        inline const TransceiverConfiguration* getTransceiverConfig() { return transceiverConfig; }
        unsigned char networkId;
    private:
        unsigned char hop;
        const MediumAccessController& mac;
        SyncStatus* syncStatus;
        const TransceiverConfiguration* const transceiverConfig;
        MACRound* currentRound = nullptr;
        MACRound* nextRound = nullptr;
    };
}

#endif /* MACCONTEXT_H */


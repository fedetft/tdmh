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
        MACContext(const MACRoundFactory* const roundFactory, const MediumAccessController& mac, bool debug = true);
        virtual ~MACContext();
        MACRound* getCurrentRound() const { return currentRound; }
        MACRound* getNextRound() const { return nextRound; }
        MACRound* shiftRound();
        inline const MediumAccessController& getMediumAccessController() { return mac; }
        void setHop(short num) { hop = num; }
        short getHop() { return hop; }
        void setNextRound(MACRound* round) { nextRound = round; }
        void initializeSyncStatus(SyncStatus* syncStatus);
        inline SyncStatus* getSyncStatus() { return syncStatus; }
        inline const TransceiverConfiguration* getTransceiverConfig() { return transceiverConfig; }
    private:
        short hop;
        const MediumAccessController& mac;
        SyncStatus* syncStatus;
        const TransceiverConfiguration* transceiverConfig;
        MACRound* currentRound;
        MACRound* nextRound;
    };
}

#endif /* MACCONTEXT_H */


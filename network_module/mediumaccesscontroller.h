/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   NetworkManager.h
 * Author: paolo
 *
 * Created on November 30, 2017, 9:51 PM
 */

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "debug_settings.h"
#include <list>

namespace miosix {
    class MACRoundFactory;
    class MACContext;
    class MediumAccessController {
    public:
        MediumAccessController() = delete;
        MediumAccessController(const MediumAccessController& orig) = delete;
        static MediumAccessController& instance(const MACRoundFactory *const roundFactory, unsigned short panId, short txPower, unsigned int radioFrequency);
        inline unsigned int getRadioFrequency() const { return baseFrequency; }
        inline short getTxPower() const { return txPower; }
        inline unsigned short getPanId() const { return panId; }
        void run();
        //5 byte (4 preamble, 1 SFD) * 32us/byte
        static const int packetPreambleTime = 160000;
        //350us and forced receiverWindow=1 fails, keeping this at minimum
        //This is dependent on the optimization level, i usually use level -O2
        static const int receivingNodeWakeupAdvance = 450000;
        inline int getOutgoingCount() { return sendQueue.size(); }
    private:
        MediumAccessController(const MACRoundFactory* const roundFactory, unsigned short panId, short txPower, unsigned int radioFrequency);
        virtual ~MediumAccessController();
        unsigned short panId;
        short txPower;
        unsigned int baseFrequency;
        MACContext* ctx;
        std::list<void*> sendQueue;
    };
}

#endif /* NETWORKMANAGER_H */


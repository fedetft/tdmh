/***************************************************************************
 *   Copyright (C)  2017 by Terraneo Federico, Polidori Paolo              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef SYNCSTATUS_H
#define SYNCSTATUS_H

#include "../macround/macround.h"
#include "floodingphase.h"
#include "time_synchronizers/synchronizer.h"
#include "time_synchronizers/flopsync2.h"
#include "interfaces-impl/virtual_clock.h"
#include "kernel/timeconversion.h"
#include <cstdint>

namespace miosix {
    struct SyncStatus {
        
        enum MacroStatus {
            IN_SYNC,
            DESYNCHRONIZED
        };
        
        SyncStatus() : SyncStatus(0, 0) {}
        SyncStatus(long long theoreticalFrameStart, long long computedFrameStart) :
                tc(new TimeConversion(EFM32_HFXO_FREQ)),
                vt(VirtualClock::instance()),
                synchronizer(new Flopsync2()),
                measuredFrameStart(computedFrameStart),
                computedFrameStart(computedFrameStart),
                theoreticalFrameStart(theoreticalFrameStart),
                clockCorrection(0),
                receiverWindow(0),
                missedPackets(0),
                internalStatus(DESYNCHRONIZED) {
            vt.setSyncPeriod(MACRound::roundDuration);
        }
        SyncStatus(const SyncStatus& orig);
        virtual ~SyncStatus() {}
        
        void initialize(int receiverWindow, long long hookPktRcvTime) {
            //Even the Theoretic is started at this time, so the absolute time is dependent of the board
            measuredFrameStart = computedFrameStart = theoreticalFrameStart = hookPktRcvTime;
            this->receiverWindow = receiverWindow;
            clockCorrection = 0;
            missedPackets = 0;
            internalStatus = IN_SYNC;
        }
        
        void next() {
            //This an uncorrected clock! Good for Rtc, that doesn't correct by itself
            theoreticalFrameStart += MACRound::roundDuration;
            //This is the estimate of the next packet in our clock
            computedFrameStart += MACRound::roundDuration + clockCorrection;
        }
        
        /*inline long long getUncorrectedTimeout() {
            return computedFrameStart + receiverWindow + MediumAccessController::packetPreambleTime;
        }
        
        inline long long getWakeupTime() {
            return computedFrameStart - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow);
        }*/
        
        inline std::pair<long long, long long> getWakeupAndTimeout(long long sendTime) {
            return std::make_pair(
                    sendTime - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow),
                    sendTime + receiverWindow + MediumAccessController::packetPreambleTime + MediumAccessController::maxPropagationDelay);
        }
        
        inline unsigned char missedPacket() {
            if(++missedPackets >= FloodingPhase::maxMissedPackets)
                internalStatus = DESYNCHRONIZED;
            else {
                this->measuredFrameStart = this->computedFrameStart;
                std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->lostPacket();
                clockCorrection = clockCorrectionReceiverWindow.first;
                receiverWindow = clockCorrectionReceiverWindow.second;
                updateVt();
            }
            return missedPackets;
        }
        
        inline void receivedPacket(long long measuredFrameStart) {
            this->measuredFrameStart = measuredFrameStart;
            std::pair<int,int> clockCorrectionReceiverWindow;
            clockCorrectionReceiverWindow = synchronizer->computeCorrection(measuredFrameStart-computedFrameStart);
            missedPackets = 0;
            clockCorrection = clockCorrectionReceiverWindow.first;
            receiverWindow = clockCorrectionReceiverWindow.second;
            updateVt();
        }
        
        inline long long correct(long long int uncorrected) {
            return tc->tick2ns(vt.uncorrected2corrected(tc->ns2tick(uncorrected)));
        }
        
        /*inline long long getTimeoutTime() {
            return correct(getUncorrectedTimeout());
        }*/
        
        inline long long getError() {
            return computedFrameStart - measuredFrameStart;
        }
        
        inline MacroStatus getInternalStatus() { return internalStatus; }
        
        TimeConversion* tc;
        VirtualClock& vt;
        Synchronizer* synchronizer;
        
        long long measuredFrameStart;
        long long computedFrameStart;
        long long theoreticalFrameStart;
    
        int clockCorrection;
        unsigned int receiverWindow;
        uint8_t missedPackets;
        
    private:
        MacroStatus internalStatus;
        inline void updateVt() {
            vt.update(tc->ns2tick(theoreticalFrameStart),tc->ns2tick(computedFrameStart), clockCorrection);
        }
    };
}

#endif /* SYNCSTATUS_H */


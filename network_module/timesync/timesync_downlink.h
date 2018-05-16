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

#pragma once

#include "../mac_phase.h"
#include "roundtrip/listening_roundtrip.h"
#include "../mac_context.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include <utility>

namespace mxnet {
class TimesyncDownlink : public MACPhase {
public:
    enum MacroStatus {
        IN_SYNC,
        DESYNCHRONIZED
    };
    TimesyncDownlink() = delete;
    TimesyncDownlink(const TimesyncDownlink& orig) = delete;
    virtual ~TimesyncDownlink() {};
    unsigned long long getDuration() const override {
        return phaseStartupTime + networkConfig.getMaxHops() * rebroadcastInterval + listeningRTP.getDuration();
    }
    unsigned getSlotsCount() const override { return 1; }
    static const int phaseStartupTime = 450000;
    static const int syncPacketSize = 11;
    static const int rebroadcastInterval = (syncPacketSize+8)*32000 + 536000; //32us per-byte + 536us total delta

    /**
     * @return the status of the synchronization state machine
     */
    MacroStatus getSyncStatus() { return internalStatus; };

    /**
     * Calculates the deepsleep deadline for speculatively sleep before sending.
     * @param tExpected the time at which the node must send the packet
     * @return the deepsleep deadline
     */
    long long getSenderWakeup(long long tExpected) {
        return tExpected - MediumAccessController::sendingNodeWakeupAdvance;
    }

    /**
     * @param tExpected the time at which the packet should be sent in the network
     * @return the deepsleep deadline and receiving timeout, calculated wrt the receiving
     * window managed by the controller
     */
    virtual std::pair<long long, long long> getWakeupAndTimeout(long long tExpected)=0;

    /**
     * @return the synchronization error
     */
    long long getError() const { return error; }

    /**
     * @return the receiving window as calculated by the controller
     */
    unsigned getReceiverWindow() const { return receiverWindow; }

    /**
     * @return the delay to the master node as calculated with the roundtrip time estimation and
     * ReverseFlooding calculation
     */
    virtual long long getDelayToMaster() const = 0;

    /**
     * Returns the slotframe start time as calculated by the FLOPSYNC-2 controller after the synchronization
     */
    virtual long long getSlotframeStart() const = 0;
    
    /**
     * \return the timesync packet counter used for slave nodes to know the
     * absolute network time
     */
    unsigned int getTimesyncPacketCounter() const
    {
        return *reinterpret_cast<const unsigned int*>(packet.data()+7);
    }
    
    bool macCanOperate() {
        return internalStatus == IN_SYNC && receiverWindow <= networkConfig.getMaxAdmittedRcvWindow();
    }

protected:
    TimesyncDownlink(MACContext& ctx, MacroStatus initStatus, unsigned receivingWindow) :
            MACPhase(ctx),
            networkConfig(ctx.getNetworkConfig()),
            listeningRTP(ctx), internalStatus(initStatus),
            receiverWindow(receivingWindow), error(0) {}
    
    TimesyncDownlink(MACContext& ctx, MacroStatus initStatus) :
            MACPhase(ctx),
            networkConfig(ctx.getNetworkConfig()),
            listeningRTP(ctx), internalStatus(initStatus),
            receiverWindow(networkConfig.getMaxAdmittedRcvWindow()), error(0) {}

    virtual void next()=0;
    virtual long long correct(long long int uncorrected)=0;
    unsigned char missedPacket();

    const NetworkConfiguration& networkConfig;
    ListeningRoundtripPhase listeningRTP;
    MacroStatus internalStatus;
    unsigned receiverWindow;
    long long error;
};

}


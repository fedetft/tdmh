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
    unsigned long long getSlotsCount() const override { return 1; }
    static const int phaseStartupTime = 450000;
    static const int syncPacketSize = 7;
    static const int rebroadcastInterval = 1016000; //32us per-byte + 600us total delta
    MacroStatus getSyncStatus() { return internalStatus; };
    long long getSenderWakeup(long long tExpected) {
        return tExpected - MediumAccessController::sendingNodeWakeupAdvance;
    }
    virtual std::pair<long long, long long> getWakeupAndTimeout(long long tExpected)=0;
    long long getError() const { return error; }
    unsigned getReceiverWindow() const { return receiverWindow; }
    virtual long long getDelayToMaster() const = 0;
    
protected:
    TimesyncDownlink(MACContext& ctx, MacroStatus initStatus, unsigned receivingWindow) :
            MACPhase(ctx),
            networkConfig(ctx.getNetworkConfig()),
            listeningRTP(ctx), internalStatus(initStatus),
            receiverWindow(receivingWindow), error(0) {};
    
    TimesyncDownlink(MACContext& ctx, MacroStatus initStatus) :
            MACPhase(ctx),
            networkConfig(ctx.getNetworkConfig()),
            listeningRTP(ctx), internalStatus(initStatus),
            receiverWindow(networkConfig.getMaxAdmittedRcvWindow()), error(0) {};
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


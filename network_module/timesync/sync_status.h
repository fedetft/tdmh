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

#include "interfaces-impl/virtual_clock.h"
#include "kernel/timeconversion.h"
#include "controller/flopsync2.h"
#include "timesync_downlink.h"

namespace mxnet {
class SyncStatus {
public:
    enum MacroStatus {
        IN_SYNC,
        DESYNCHRONIZED
    };
    SyncStatus() = delete;
    virtual ~SyncStatus() {};
    SyncStatus(const SyncStatus&) = delete;
    SyncStatus(const NetworkConfiguration* const config) : SyncStatus(config, DESYNCHRONIZED) {};
    SyncStatus(const NetworkConfiguration* const config, MacroStatus status) : config(config), internalStatus(status) {};
    virtual void initialize(long long hookPktTime)=0;
    virtual void next()=0;
    virtual long long getSenderWakeup(long long tExpected) {
        return tExpected - MediumAccessController::sendingNodeWakeupAdvance;
    }
    virtual std::pair<long long, long long> getWakeupAndTimeout(long long tExpected)=0;
    virtual long long correct(long long int uncorrected)=0;
    virtual long long getError()=0;
    MacroStatus getInternalStatus() { return internalStatus; };
protected:
    MacroStatus internalStatus;
    const NetworkConfiguration* const config;
};

class MasterSyncStatus : public SyncStatus {
public:
    MasterSyncStatus() = delete;
    virtual ~MasterSyncStatus() {};
    MasterSyncStatus(const MasterSyncStatus&) = delete;
    MasterSyncStatus(const NetworkConfiguration* const config) :
        SyncStatus(config, IN_SYNC) {};
    void initialize(long long hookPktTime) override {
        slotframeTime = hookPktTime;
    }
    void next() override {
        slotframeTime += config->getSlotframeDuration();
    }
    std::pair<long long, long long> getWakeupAndTimeout(long long tExpected) override {
        return std::make_pair(
            tExpected - (MediumAccessController::receivingNodeWakeupAdvance + config->getMaxAdmittedRcvWindow()),
            tExpected + config->getMaxAdmittedRcvWindow() + MediumAccessController::packetPreambleTime +
            MediumAccessController::maxPropagationDelay
        );
    }
    long long correct(long long int uncorrected) override {
        return uncorrected;
    }
    long long getError() override {
        return 0;
    }
private:
    long long slotframeTime;
};

class DynamicSyncStatus : public SyncStatus {
public:
    DynamicSyncStatus() = delete;
    DynamicSyncStatus(const SyncStatus& orig) = delete;
    virtual ~DynamicSyncStatus() {};
    DynamicSyncStatus(const NetworkConfiguration* const config) :
        SyncStatus(config),
        tc(new miosix::TimeConversion(EFM32_HFXO_FREQ)),
        vt(miosix::VirtualClock::instance()),
        synchronizer(new Flopsync2()),
        measuredFrameStart(0),
        computedFrameStart(0),
        theoreticalFrameStart(0),
        clockCorrection(0),
        receiverWindow(0),
        missedPackets(0) {
            vt.setSyncPeriod(config->getSlotframeDuration());
    }

    void initialize(long long hookPktTime) override {
        //Even the Theoretic is started at this time, so the absolute time is dependent of the board
        measuredFrameStart = computedFrameStart = theoreticalFrameStart = hookPktTime;
        receiverWindow = synchronizer->getReceiverWindow();
        clockCorrection = 0;
        missedPackets = 0;
        internalStatus = IN_SYNC;
    }

    void next() override {
        //This an uncorrected clock! Good for Rtc, that doesn't correct by itself
        //needed because we ALWAYS need to consider the reference to be the first hook time,
        //otherwise we would build a second integrator without actually managing it.
        theoreticalFrameStart += config->getSlotframeDuration();
        //This is the estimate of the next packet in our clock
        //using the FLOPSYNC-2 clock correction
        computedFrameStart += config->getSlotframeDuration() + clockCorrection;
    }

    std::pair<long long, long long> getWakeupAndTimeout(long long tExpected) override {
        return std::make_pair(
                tExpected - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow),
                tExpected + receiverWindow + MediumAccessController::packetPreambleTime +
                MediumAccessController::maxPropagationDelay
                );
    }

    unsigned char missedPacket();

    void receivedPacket(long long arrivalTime);

    long long correct(long long int uncorrected) override {
        //TODO FIXME this works by converting, applying the correction and converting back.
        //This leads to a non-negligible error.
        return tc->tick2ns(vt.uncorrected2corrected(tc->ns2tick(uncorrected)));
    }

    long long getError() override {
        return computedFrameStart - measuredFrameStart;
    }

    bool checkExpired(long long now, long long tExpected) {
        return now >= tExpected - receiverWindow;
    }

private:
    miosix::TimeConversion* const tc;
    miosix::VirtualClock& vt;
    Synchronizer* const synchronizer;

    long long measuredFrameStart;
    long long computedFrameStart;
    long long theoreticalFrameStart;

    int clockCorrection;
    unsigned int receiverWindow;
    unsigned char missedPackets;

    void updateVt() {
        vt.update(
                tc->ns2tick(theoreticalFrameStart),
                tc->ns2tick(computedFrameStart), clockCorrection
        );
    }
};
}


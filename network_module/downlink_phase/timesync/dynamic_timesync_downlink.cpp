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

#include "dynamic_timesync_downlink.h"
#include "networktime.h"
#include "../../uplink_phase/dynamic_uplink_phase.h"
#include "../../data_phase/dataphase.h"
#include "../../util/debug_settings.h"
#include <cassert>

using namespace miosix;

namespace mxnet {

void DynamicTimesyncDownlink::periodicSync() {
    greenLed::high();
    //slotStart = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    auto correctedStart = correct(computedFrameStart);
    Packet pkt;
    using namespace std::placeholders;
    auto pred = std::bind(&DynamicTimesyncDownlink::isSyncPacket, this, _1, _2, true);
    auto rcvResult = pkt.recv(ctx, correctedStart, pred, Transceiver::Correct::UNCORR);
    if(rcvResult.error != RecvResult::ErrorCode::OK) {
        // Turn off the radio because we don't need it anymore this round
        ctx.transceiverIdle();
        auto n = missedPacket();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            auto nt = NetworkTime::fromLocalTime(getSlotframeStart());
            print_dbg("[T] miss NT=%lld u=%d w=%d\n", nt.get(), clockCorrection, receiverWindow);
            if (n >= networkConfig.getMaxMissedTimesyncs())
                print_dbg("[T] lost sync\n");
        }
    }
    else {
        //corrected time in NS, to pass to transceiver
        pkt[2]++;
        //Rebroadcast the sync packet
        measuredFrameStart = correct(rcvResult.timestamp);
        rebroadcast(pkt, measuredFrameStart);
        ctx.transceiverIdle();
        // TODO: make a struct containing the packetCounter
        packetCounter++;
        auto newPacketCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
        if(newPacketCounter != packetCounter)
            print_dbg("[T] Received wrong packetCounter=%d (should be %d)", newPacketCounter, packetCounter);

        error = rcvResult.timestamp - computedFrameStart;
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->computeCorrection(error);
        missedPackets = 0;
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
        updateVt();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {            
            auto nt = NetworkTime::fromLocalTime(getSlotframeStart());
            print_dbg("[T] hop=%u NT=%lld ets=%lld ats=%lld e=%lld u=%d w=%d rssi=%d\n",
                      pkt[2],
                      nt.get(),
                      correctedStart,
                      rcvResult.timestamp,
                      error,
                      clockCorrection,
                      receiverWindow,
                      rcvResult.rssi);
        }
    }
    greenLed::low();
}

std::pair<long long, long long> DynamicTimesyncDownlink::getWakeupAndTimeout(long long tExpected) {
    return std::make_pair(
            tExpected - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow),
            tExpected + receiverWindow + MediumAccessController::packetPreambleTime +
            MediumAccessController::maxPropagationDelay
            );
}

void DynamicTimesyncDownlink::resyncTime() {
    //Even the Theoretic is started at this time, so the absolute time is dependent of the board
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] Resync\n");

    greenLed::high();

    Packet pkt;
    using namespace std::placeholders;
    auto pred = std::bind(&DynamicTimesyncDownlink::isSyncPacket, this, _1, _2, false);
    auto rcvResult = pkt.recv(ctx, infiniteTimeout, pred, Transceiver::Correct::UNCORR);

    greenLed::low();
    ++pkt[2];
    auto myHop = pkt[2];
    
    // NOTE: by the way the virtual clock is currently implemented, a clock
    // jump is observed whenever updateVt is called after a reset, due to the
    // resetting of theoreticalFrameStart. This will have to be fixed in the
    // virtual clock. Until this is done, however, we put this clock jump in the
    // most convenient place, which is before the first virtual clock use
    // after a resync (which is the "correct(rcvResult.timestamp)" line).
    // By doing so we do not interfere with the duration of the first sync
    // period, which would mess up the deadbeat in FLOPSYNC2 (recoverable),
    // but most importantly we do not get an unrecoverable clock error in
    // NetworkTime::setLocalNodeToNetworkTimeOffset.
    reset(rcvResult.timestamp);
    updateVt();
    
    measuredFrameStart = correct(rcvResult.timestamp);
    rebroadcast(pkt, measuredFrameStart);
    ctx.transceiverIdle();

    ctx.setHop(pkt[2]);
    auto slotframeStart = getSlotframeStart();

    // NOTE: call resetMAC to clear the status of all the MAC components after resync
    resyncMAC();

    packetCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
    NetworkTime::setLocalNodeToNetworkTimeOffset(packetCounter *
                                                 networkConfig.getClockSyncPeriod() - slotframeStart);
    auto ntNow = NetworkTime::fromLocalTime(slotframeStart);
    ctx.getUplink()->alignToNetworkTime(ntNow);

    if (ENABLE_TIMESYNC_DL_INFO_DBG)      
        print_dbg("[T] hop=%d NT=%lld ats=%lld w=%d rssi=%d\n",
                  pkt[2], ntNow.get(), rcvResult.timestamp, receiverWindow, rcvResult.rssi
        );
}

inline void DynamicTimesyncDownlink::execute(long long slotStart)
{
    //the argument is ignored, since this is the time source class.
    next();
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if (internalStatus == DESYNCHRONIZED) {
        resyncTime();
    } else {
        periodicSync();
    }
}

void DynamicTimesyncDownlink::rebroadcast(const Packet& pkt, long long arrivalTs){
    if(pkt[2] == networkConfig.getMaxHops()) return;
    pkt.send(ctx, arrivalTs + rebroadcastInterval);
}

void DynamicTimesyncDownlink::reset(long long hookPktTime) {
    //All the timestamps start from here, since i take this points as a ground reference
    //so no need to correct anything.
    synchronizer->reset();
    computedFrameStart = theoreticalFrameStart = hookPktTime;
    receiverWindow = synchronizer->getReceiverWindow();
    clockCorrection = 0;
    missedPackets = 0;
    error = 0;
    internalStatus = IN_SYNC;
}

void DynamicTimesyncDownlink::next() {
    //This an uncorrected clock! Good for Rtc, that doesn't correct by itself
    //needed because we ALWAYS need to consider the reference to be the first hook time,
    //otherwise we would build a second integrator without actually managing it.
    theoreticalFrameStart += networkConfig.getClockSyncPeriod();
    //This is the estimate of the next packet in our clock
    //using the FLOPSYNC-2 clock correction
    computedFrameStart += networkConfig.getClockSyncPeriod() + clockCorrection;
}

long long DynamicTimesyncDownlink::correct(long long int uncorrected) {
    //TODO FIXME this works by converting, applying the correction and converting back.
    //This leads to a non-negligible error.
    return tc->tick2ns(vt.uncorrected2corrected(tc->ns2tick(uncorrected)));
}

unsigned char DynamicTimesyncDownlink::missedPacket() {
    // We have not received the sync packet but we need to increment it
    // to keep the NetworkTime up to date
    packetCounter++;
    // NOTE: It is important that measuredFrameStart is a CORRECTED
    // time, because it is used as is in the NetworkTime
    measuredFrameStart = correct(computedFrameStart);

    if(++missedPackets >= networkConfig.getMaxMissedTimesyncs()) {
        internalStatus = DESYNCHRONIZED;
        synchronizer->reset();
        desyncMAC();
        // NOTE: when we desynchronize, we set the correction to zero.
        // This is important, as we may spend an unbounded time desynchronized.
        // Why is this a problem? The virtual clock has separate conversion
        // factors for the forward and backward correction (see factor and
        // inverseFactor in virtual_clock.h). As the time from the last
        // correction passes, the round trip (between a corrected to
        // an uncorrected time and back) conversion error grows, and if its
        // sign is negative, the scheduler timer will no longer recognize the
        // wakeup it has set and won't wake up threads, effectively deadlocking.
        // The same issue has been observed in the nanosecond to tick conversion
        // in timeconversion.cpp. However, while in the tick/ns the issue is
        // unavoidable and the code has been hardened to handle this case,
        // it has been decided that it's too computationally costly to do the
        // same in this case. For this reason, when we desynchronize, we reset
        // clockCorrection to zero and call updateVt(). This resets the
        // conversion coefficents to 1.0 both ways, which is perfectly
        // symmetrical, making the round trip work even for unbounded desyncs.
        clockCorrection = 0;
    } else {
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->lostPacket();
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
    }
    updateVt();
    return missedPackets;
}

void DynamicTimesyncDownlink::resyncMAC() {
    ctx.getUplink()->resync();
    ctx.getScheduleDistribution()->resync();
    ctx.getDataPhase()->resync();
    ctx.getStreamManager()->resync();
}

void DynamicTimesyncDownlink::desyncMAC() {
    ctx.getUplink()->desync();
    ctx.getScheduleDistribution()->desync();
    ctx.getDataPhase()->desync();
    ctx.getStreamManager()->desync();
}

}

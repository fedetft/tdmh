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
#include "../uplink/topology/topology_context.h"
#include "../uplink/uplink_phase.h"
#include "../data/dataphase.h"
#include "../debug_settings.h"
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
        auto n = missedPacket();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            print_dbg("[T] miss u=%d w=%d\n", clockCorrection, receiverWindow);
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
        packetCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
        error = rcvResult.timestamp - computedFrameStart;
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->computeCorrection(error);
        missedPackets = 0;
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
        updateVt();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            print_dbg("[T] hop=%u ets=%lld ats=%lld e=%lld u=%d w=%d Mts=%lld rssi=%d\n",
                    pkt[2],
                    correctedStart,
                    rcvResult.timestamp,
                    error,
                    clockCorrection,
                    receiverWindow,
                    measuredFrameStart - pkt[2] * rebroadcastInterval,
                   rcvResult.rssi);
        }
    }
    greenLed::low();
    ctx.transceiverIdle(); //Save power waiting for rebroadcast time
}

std::pair<long long, long long> DynamicTimesyncDownlink::getWakeupAndTimeout(long long tExpected) {
    return std::make_pair(
            tExpected - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow),
            tExpected + receiverWindow + MediumAccessController::packetPreambleTime +
            MediumAccessController::maxPropagationDelay
            );
}

void DynamicTimesyncDownlink::resync() {
    //Even the Theoretic is started at this time, so the absolute time is dependent of the board
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] Resync\n");
    //TODO: attach to strongest signal, not just to the first received packet

    greenLed::high();

    Packet pkt;
    using namespace std::placeholders;
    auto pred = std::bind(&DynamicTimesyncDownlink::isResyncPacket, this, _1, _2);
    auto rcvResult = pkt.recv(ctx, infiniteTimeout, pred, Transceiver::Correct::UNCORR);

    greenLed::low();
    auto start = rcvResult.timestamp - pkt[2] * rebroadcastInterval;
    ++pkt[2];
    reset(rcvResult.timestamp);
    ctx.setHop(pkt[2]);
    auto correctPacketTime = correct(rcvResult.timestamp);
    rebroadcast(pkt, correctPacketTime);

    ctx.transceiverIdle();

    // TODO: make a struct containing the packetCounter
    packetCounter = *reinterpret_cast<unsigned int*>(&pkt[7]);
    NetworkTime::setLocalNodeToNetworkTimeOffset(getTimesyncPacketCounter() * networkConfig.getClockSyncPeriod() - correct(start));
    auto ntNow = NetworkTime::fromLocalTime(correctPacketTime);
    ctx.getUplink()->alignToNetworkTime(ntNow);
    ctx.getDataPhase()->alignToNetworkTime(ntNow);

    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[F] hop=%d ats=%lld w=%d mst=%lld rssi=%d\n",
                pkt[2], rcvResult.timestamp, receiverWindow, start, rcvResult.rssi
        );

    static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->changeHop(pkt[2]);
}

inline void DynamicTimesyncDownlink::execute(long long slotStart)
{
    //the argument is ignored, since this is the time source class.
    next();
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    if (internalStatus == DESYNCHRONIZED) {
        resync();
    } else {
        periodicSync();
        //TODO implement roundtrip nodes list
        //if (false && static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->hasSuccessor(0))
            //a successor of the current node needs to perform RTT estimation
            //listeningRTP.execute(slotStart + RoundtripSubphase::senderDelay);
        //else if (false)
            //i can perform RTT estimation
            //askingRTP.execute(slotStart + RoundtripSubphase::senderDelay);
    }
    ctx.transceiverIdle();
}

void DynamicTimesyncDownlink::rebroadcast(const Packet& pkt, long long arrivalTs){
    if(pkt[2] == networkConfig.getMaxHops()) return;
    pkt.send(ctx, arrivalTs + rebroadcastInterval);
}

void DynamicTimesyncDownlink::reset(long long hookPktTime) {
    //All the timestamps start from here, since i take this points as a ground reference
    //so no need to correct anything.
    synchronizer->reset();
    measuredFrameStart = computedFrameStart = theoreticalFrameStart = hookPktTime;
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
    if(++missedPackets >= networkConfig.getMaxMissedTimesyncs()) {
        internalStatus = DESYNCHRONIZED;
        synchronizer->reset();
    } else {
        measuredFrameStart = computedFrameStart;
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->lostPacket();
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
        updateVt();
    }
    return missedPackets;
}

}

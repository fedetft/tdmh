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
#include "../debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicTimesyncDownlink::periodicSync() {
    //slotStart = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    auto correctedStart = correct(computedFrameStart);
    auto wakeupTimeout = getWakeupAndTimeout(correctedStart);
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] WU=%lld TO=%lld\n", wakeupTimeout.first, wakeupTimeout.second);
    auto now = getTime();//TODO look for uncorrected getTime to perform correct comparisons
    //check if we missed the deadline for the synchronization time
    if (now + receiverWindow >= correctedStart){
        if (ENABLE_TIMESYNC_ERROR_DBG)
            print_dbg("[T] 2 l8\n");
        missedPacket();
        return;
    }
    //sleep, if we have time
    if(now < wakeupTimeout.first)
        ctx.sleepUntil(wakeupTimeout.first);
    do {
        rcvResult = ctx.recv(packet.data(), syncPacketSize, wakeupTimeout.second, Transceiver::Correct::UNCORR);
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size){
                print_dbg("[T] err=%d D=%d", rcvResult.error, rcvResult.size);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("[T] TO!\n");
        }
    } while(!isSyncPacket() && rcvResult.error != RecvResult::ErrorCode::TIMEOUT);

    ctx.transceiverIdle(); //Save power waiting for rebroadcast time

    if (rcvResult.error != RecvResult::ErrorCode::TIMEOUT) {
        //corrected time in NS, to pass to transceiver
        packet[2]++;
        //Rebroadcast the sync packet
        measuredFrameStart = correct(rcvResult.timestamp);
        rebroadcast(measuredFrameStart);
        ctx.transceiverIdle();
        error = rcvResult.timestamp - computedFrameStart;
        std::pair<int,int> clockCorrectionReceiverWindow = synchronizer->computeCorrection(error);
        missedPackets = 0;
        clockCorrection = clockCorrectionReceiverWindow.first;
        receiverWindow = clockCorrectionReceiverWindow.second;
        updateVt();
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            print_dbg("[T] ets=%lld ats=%lld e=%lld u=%d w=%d Mts=%lld rssi=%d\n",
                    correctedStart,
                    rcvResult.timestamp,
                    error,
                    clockCorrection,
                    receiverWindow,
                    measuredFrameStart - packet[2] * rebroadcastInterval,
                   rcvResult.rssi);
        }
    } else {
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            print_dbg("[T] miss u=%d w=%d\n", clockCorrection, receiverWindow);
            if (missedPacket() >= networkConfig.getMaxMissedTimesyncs())
                print_dbg("[T] lost sync\n");
        }
    }
}

std::pair<long long, long long> DynamicTimesyncDownlink::getWakeupAndTimeout(long long tExpected) {
    return std::make_pair(
            tExpected - (MediumAccessController::receivingNodeWakeupAdvance + receiverWindow),
            tExpected + receiverWindow + MediumAccessController::packetPreambleTime +
            MediumAccessController::maxPropagationDelay
            );
}

void DynamicTimesyncDownlink::resync() {
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
    //Even the Theoretic is started at this time, so the absolute time is dependent of the board
        print_dbg("[T] Resync\n");
    //TODO: attach to strongest signal, not just to the first received packet
    for (bool success = false; !success; success = isSyncPacket()) {
        rcvResult = ctx.recv(packet.data(), syncPacketSize, infiniteTimeout, Transceiver::Correct::UNCORR);
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size){
                print_dbg("Received packet, error %d, size %d, timestampValid %d: ",
                        rcvResult.error, rcvResult.size, rcvResult.timestampValid
                );
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("No packet received, timeout reached\n");
        }
    }
    auto start = rcvResult.timestamp - packet[2] * rebroadcastInterval;
    ++packet[2];
    reset(rcvResult.timestamp);
    ctx.setHop(packet[2]);
    rebroadcast(correct(rcvResult.timestamp));

    ledOff();
    ctx.transceiverIdle();
    
    NetworkTime::setLocalNodeToNetworkTimeOffset(getTimesyncPacketCounter() * networkConfig.getClockSyncPeriod() - correct(start));

    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[F] hop=%d ats=%lld w=%d mst=%lld rssi=%d\n",
                packet[2], rcvResult.timestamp, receiverWindow, start, rcvResult.rssi
        );

    static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->setMasterAsNeighbor(packet[2] == 1);
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
        if (false && static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->hasSuccessor(0))
            //a successor of the current node needs to perform RTT estimation
            listeningRTP.execute(slotStart + RoundtripSubphase::senderDelay);
        else if (false)
            //i can perform RTT estimation
            askingRTP.execute(slotStart + RoundtripSubphase::senderDelay);
    }
    ctx.transceiverIdle();
}

void DynamicTimesyncDownlink::rebroadcast(long long arrivalTs){
    if(packet[2] == networkConfig.getMaxHops()) return;
    ctx.sendAt(packet.data(), syncPacketSize, arrivalTs + rebroadcastInterval);
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

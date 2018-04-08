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
#include "../uplink/topology/topology_context.h"
#include "../debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicTimesyncDownlink::periodicSync(long long slotStart) {
    slotStart = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    auto* dSyncStatus = static_cast<DynamicSyncStatus*>(syncStatus);
    //This is fully corrected
    auto wakeupTimeout = syncStatus->getWakeupAndTimeout(syncStatus->correct(slotStart));
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] WU=%lld TO=%lld\n", wakeupTimeout.first, wakeupTimeout.second);
    bool success = false;
    auto now = getTime();
    //check if we missed the deadline for the synchronization time
    if (dynamic_cast<DynamicSyncStatus*>(syncStatus)->checkExpired(now, slotStart)) {
        if (ENABLE_FLOODING_ERROR_DBG)
            print_dbg("[T] 2 l8\n");
        dSyncStatus->missedPacket();
        return;
    }
    //sleep, if we have time
    if(now < wakeupTimeout.first)
        pm.deepSleepUntil(wakeupTimeout.first);
    for (; !(success || rcvResult.error == RecvResult::ErrorCode::TIMEOUT);
            success = isSyncPacket()) {
        try {
            //uncorrected TS needed for computing the correction with flopsync
            rcvResult = transceiver.recv(packet.data(), syncPacketSize, wakeupTimeout.second, Transceiver::Unit::NS, Transceiver::Correct::UNCORR);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size){
                print_dbg("[T] err=%d D=%d", rcvResult.error, rcvResult.size);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("[T] TO!\n");
        }
    }

    transceiver.idle(); //Save power waiting for rebroadcast time

    if (success) {
        //This conversion is really necessary to get the corrected time in NS, to pass to transceiver
        packet[2]++;
        //Rebroadcast the sync packet
        dSyncStatus->receivedPacket(rcvResult.timestamp);
        auto correctedMeasuredTimestamp = syncStatus->correct(rcvResult.timestamp);
        rebroadcast(correctedMeasuredTimestamp);
        transceiver.turnOff();
        long long measuredGlobalFirstActivityTime = correctedMeasuredTimestamp - packet[2] * rebroadcastInterval;
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            auto cData = dSyncStatus->getControllerData();
            print_dbg("[T] ats=%lld e=%lld u=%d w=%d mts=%lld rssi=%d\n",
                    rcvResult.timestamp,
                    syncStatus->getError(),
                    cData.first,
                    cData.second,
                    measuredGlobalFirstActivityTime,
                   rcvResult.rssi);
        }
    } else {
        if (ENABLE_TIMESYNC_DL_INFO_DBG) {
            auto cData = dSyncStatus->getControllerData();
            print_dbg("[T] miss u=%d w=%d\n", cData.first, cData.second);
            if (dSyncStatus->missedPacket() >= networkConfig->getMaxMissedTimesyncs())
                print_dbg("[T] lost sync\n");
        }
    }
}

void DynamicTimesyncDownlink::resync() {
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[T] Resync\n");
    //TODO: attach to strongest signal, not just to the first received packet
    for (bool success = false; !success; success = isSyncPacket()) {
        try {
            rcvResult = transceiver.recv(packet.data(), syncPacketSize, infiniteTimeout);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
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
    syncStatus->initialize(rcvResult.timestamp);
    ctx.setHop(packet[2]);
    rebroadcast(syncStatus->correct(rcvResult.timestamp));

    ledOff();
    transceiver.turnOff();

    auto cData = static_cast<DynamicSyncStatus*>(syncStatus)->getControllerData();
    if (ENABLE_TIMESYNC_DL_INFO_DBG)
        print_dbg("[F] hop=%d ats=%lld w=%d mts=%lld rssi=%d\n",
                packet[2], rcvResult.timestamp, cData.second, start, rcvResult.rssi
        );

    static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->setMasterAsNeighbor(packet[2] == 1);
}

inline void DynamicTimesyncDownlink::execute(long long slotStart)
{
    transceiver.configure(ctx.getTransceiverConfig());
    transceiver.turnOn();
    if (syncStatus->getInternalStatus() == SyncStatus::DESYNCHRONIZED) {
        resync();
        return;
    }
    periodicSync(slotStart);
    //TODO implement roundtrip nodes list
    if (false && static_cast<DynamicTopologyContext*>(ctx.getTopologyContext())->hasSuccessor(0))
        //a successor of the current node needs to perform RTT estimation
        listeningRTP.execute(slotStart + RoundtripSubphase::senderDelay);
    else if (false)
        //i can perform RTT estimation
        askingRTP.execute(slotStart + RoundtripSubphase::senderDelay);
    transceiver.turnOff();
}
}


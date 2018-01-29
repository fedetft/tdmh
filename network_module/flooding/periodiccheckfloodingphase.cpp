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

#include "periodiccheckfloodingphase.h"
#include "hookingfloodingphase.h"
#include <stdio.h>

namespace miosix{
PeriodicCheckFloodingPhase::~PeriodicCheckFloodingPhase() {
}
void PeriodicCheckFloodingPhase::execute(MACContext& ctx)
{
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("Synchronizing...\n");
#endif /* ENABLE_FLOODING_INFO_DBG */
    
    syncStatus = ctx.getSyncStatus();
    
    //This is fully corrected
    auto wakeupTimeout = syncStatus->getWakeupAndTimeout(localFirstActivityTime);
    auto timeoutTime = syncStatus->correct(wakeupTimeout.second);
    
    //Transceiver configured with non strict timeout
    transceiver.configure(*ctx.getTransceiverConfig());
    unsigned char packet[syncPacketSize];
    
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("WU @ %lld\n", wakeupTimeout.first);
    printf("TO %lld\n", timeoutTime);
#endif /* ENABLE_FLOODING_INFO_DBG */

    ledOn();
    transceiver.turnOn();
    
    auto now = getTime();
    //check if we skipped the synchronization time
    if (now >= localFirstActivityTime - syncStatus->receiverWindow) {
#ifdef ENABLE_FLOODING_ERROR_DBG
        printf("PeriodicFloodingCheck started too late\n");
#endif /* ENABLE_FLOODING_ERROR_DBG */
        syncStatus->missedPacket();
        return;
    }
    if(now < wakeupTimeout.first)
        pm.deepSleepUntil(wakeupTimeout.first);
    
    RecvResult result;
    bool success = false;
    for (; !(success || result.error == RecvResult::ErrorCode::TIMEOUT);
            success = isSyncPacket(result, packet, ctx.getMediumAccessController().getPanId(), ctx.getHop())) {
        try {
            //uncorrected TS needed for computing the correction with flopsync
            result = transceiver.recv(packet, syncPacketSize, timeoutTime, Transceiver::Unit::NS, HardwareTimer::Correct::UNCORR);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_PKT_INFO_DBG
        if(result.size){
            printf("Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
#ifdef ENABLE_PKT_DUMP_DBG
            memDump(packet, result.size);
#endif /* ENABLE_PKT_DUMP_DBG */
        } else printf("No packet received, timeout reached\n");
#endif /* ENABLE_PKT_INFO_DBG */
    }
    
    transceiver.idle(); //Save power waiting for rebroadcast time
    
    //This conversion is really necessary to get the corrected time in NS, to pass to transceiver
    long long correctedMeasuredFrameStart = syncStatus->correct(result.timestamp);
    //Rebroadcast the sync packet
    if (success) rebroadcast(correctedMeasuredFrameStart, packet);
    consumed = true;
    transceiver.turnOff();
    ledOff();
    
    if (success) {
        syncStatus->receivedPacket(result.timestamp);
#ifdef ENABLE_FLOODING_INFO_DBG
        printf("sync time: [%lld]\n", result.timestamp);
        printf("e=%lld u=%d w=%d rssi=%d\n",
                syncStatus->getError(),
                syncStatus->clockCorrection,
                syncStatus->receiverWindow, 
               result.rssi);
    } else {
        printf("miss u=%d w=%d\n", syncStatus->clockCorrection, syncStatus->receiverWindow);
        if (syncStatus->missedPacket() >= maxMissedPackets)
        {
            printf("Lost sync\n");
        }
#endif /* ENABLE_FLOODING_INFO_DBG */
    }
}
}


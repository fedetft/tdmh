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
    if (debug) printf("Synchronizing...\n");
#endif /* ENABLE_FLOODING_INFO_DBG */
    
    auto* status = ctx.getSyncStatus();
    
    //This is fully corrected
    long long timeoutTime = status->getTimeoutTime();  
    
    //check if we skipped the synchronization time
    if (getTime() >= startTime) {
#ifdef ENABLE_FLOODING_ERROR_DBG
        if (debug) printf("PeriodicFloodingCheck started too late\n");
#endif /* ENABLE_FLOODING_ERROR_DBG */
        status->missedPacket();
        return;
    }
    
    //Transceiver configured with non strict timeout
    transceiver.configure(*ctx.getTransceiverConfig());
    unsigned char packet[syncPacketSize];
    
    //Awaiting a time sync packet
    bool timeout = false;
    
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("Will wake up @ %lld\n", startTime);
    printf("Will await sync packet until %lld (uncorrected)\n", timeoutTime);
#endif /* ENABLE_FLOODING_INFO_DBG */

    ledOn();
    transceiver.turnOn();
    pm.deepSleepUntil(startTime);
    
    RecvResult result;
    for (bool success = false; !(success || timeout);) {
        try {    
            result = transceiver.recv(packet, syncPacketSize, timeoutTime, Transceiver::Unit::NS, HardwareTimer::Correct::UNCORR);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            if(debug) printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_PKT_INFO_DBG
        if (debug){
            if(result.size){
                printf("Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
#ifdef ENABLE_PKT_DUMP_DBG
                memDump(packet, result.size);
#endif /* ENABLE_PKT_DUMP_DBG */
            } else printf("No packet received, timeout reached\n");
        }
#endif /* ENABLE_PKT_INFO_DBG */
        if (isSyncPacket(result, packet) && packet[2] == ctx.getHop() - 1)
        {
            success = true;
        } else {
            if (getTime() >= timeoutTime)
                timeout = true;
        }
    }
    
    transceiver.idle(); //Save power waiting for rebroadcast time
    
    //This conversion is really necessary to get the corrected time in NS, to pass to transceiver
    long long correctedMeasuredFrameStart = status->correct(result.timestamp);
    //Rebroadcast the sync packet
    if (!timeout) rebroadcast(correctedMeasuredFrameStart, packet);
    transceiver.turnOff();
    ledOff();
    
    if (timeout) {
#ifdef ENABLE_FLOODING_INFO_DBG
        printf("sync time: [%lld]\n", result.timestamp);
#endif /* ENABLE_FLOODING_INFO_DBG */
        if (status->missedPacket() >= maxMissedPackets)
        {
#ifdef ENABLE_FLOODING_INFO_DBG
            printf("Lost sync\n");
#endif /* ENABLE_FLOODING_INFO_DBG */
            return;
        }
    } else
        status->receivedPacket(result.timestamp);
#ifdef ENABLE_FLOODING_INFO_DBG
    if (debug) {
        if (timeout)
            printf("miss u=%d w=%d\n", status->clockCorrection, status->receiverWindow);
        else
            printf("e=%lld u=%d w=%d rssi=%d\n",
                    status->getError(),
                    status->clockCorrection,
                    status->receiverWindow, 
                   result.rssi);
    }
#endif /* ENABLE_FLOODING_INFO_DBG */
}
}


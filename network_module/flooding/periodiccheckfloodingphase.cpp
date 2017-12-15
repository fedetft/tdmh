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
    if (debug) printf("Synchronizing...\n");
    
    auto* status = ctx.getSyncStatus();
    
    //This is fully corrected
    long long timeoutTime = status->getTimeoutTime();  
    
    //check if we skipped the synchronization time
    if (getTime() >= startTime) {
        if (debug) printf("PeriodicFloodingCheck started too late\n");
        status->missedPacket();
        return;
    }
    
    //Transceiver configured with non strict timeout
    transceiver.configure(*ctx.getTransceiverConfig());
    unsigned char packet[syncPacketSize];
    
    //Awaiting a time sync packet
    bool timeout = false;
    
    printf("Will wake up @ %lld\n", startTime);
    printf("Will await sync packet until %lld (uncorrected)\n", timeoutTime);

    ledOn();
    pm.deepSleepUntil(startTime);
    
    transceiver.turnOn();
    
    RecvResult result;
    for (bool success = false; !(success || timeout);) {
        if (debug) printf("Awaiting sync packet...\n");
        try {    
            result = transceiver.recv(packet, syncPacketSize, timeoutTime, Transceiver::Unit::NS, HardwareTimer::Correct::UNCORR);
        } catch(std::exception& e) {
            if(debug) printf("%s\n", e.what());
        }
        /*if (debug){
            if(result.size){
                printf("Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
                memDump(packet, result.size);
            } else printf("No packet received, timeout reached\n");
        }*/
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
    
    printf("sync time: [%lld]\n", result.timestamp);
    if (timeout) {
        if (status->missedPacket() >= maxMissedPackets)
        {
            printf("Lost sync\n");
            return;
        }
    } else
        status->receivedPacket(result.timestamp);
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
}
}


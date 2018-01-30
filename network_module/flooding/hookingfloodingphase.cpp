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

#include "hookingfloodingphase.h"
#include "periodiccheckfloodingphase.h"

namespace miosix {

HookingFloodingPhase::~HookingFloodingPhase() {
}

void HookingFloodingPhase::execute(MACContext& ctx)
{   
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("[F] Resync\n");
#endif /* ENABLE_FLOODING_INFO_DBG */
    auto* status = ctx.getSyncStatus();
    auto* synchronizer = status->synchronizer;
    synchronizer->reset();
    transceiver.configure(*ctx.getTransceiverConfig());
    transceiver.turnOn();
    unsigned char packet[syncPacketSize];
    //TODO: attach to strongest signal, not just to the first received packet
    RecvResult result;
    ledOn();
    for (bool success = false; !success; success = isSyncPacket(result, packet, ctx.getNetworkConfig()->panId)) {
        try {
            result = transceiver.recv(packet, syncPacketSize, infiniteTimeout);
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
    ++packet[2];
    rebroadcast(result.timestamp, packet);
    
    ledOff();
    transceiver.turnOff();
    
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("[F] ERT=%lld, RT=%lld\n", status->computedFrameStart, result.timestamp);
#endif /* ENABLE_FLOODING_INFO_DBG */
    
    status->initialize(synchronizer->getReceiverWindow(), result.timestamp);
    ctx.setHop(packet[2]);
    
    //Correct frame start considering hops
    //measuredFrameStart-=hop*packetRebroadcastTime;
#ifdef ENABLE_FLOODING_INFO_DBG
    printf("[F] hop=%d, w=%d, rssi=%d\n", packet[2], status->receiverWindow, result.rssi);
#endif /* ENABLE_FLOODING_INFO_DBG */
}
}


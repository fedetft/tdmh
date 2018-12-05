/***************************************************************************
 *   Copyright (C)  2017 by Polidori Paolo, Terraneo Federico,             *
 *                          Riccardi Fabiano                               *
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

#include <stdio.h>

#include "led_bar.h"
#include "listening_roundtrip.h"
#include "../timesync_downlink.h"
#include "../../debug_settings.h"

using namespace miosix;

namespace mxnet {

void ListeningRoundtripPhase::execute(long long slotStart) {
    throw std::runtime_error("ListeningRoundtripPhase::execute: This code should never be called");

/*    //maybe with a different class for the master node?
    long long timeoutTime = slotStart + receiverWindow + MediumAccessController::maxPropagationDelay + MediumAccessController::packetPreambleTime;
    
    bool success = false;
    Packet pkt;
    for(; !(success || rcvResult.error == miosix::RecvResult::TIMEOUT); success = isRoundtripAskPacket()) {
        rcvResult = ctx.recv(packet.data(), replyPacketSize, timeoutTime);
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size){
                print_dbg("[RTT] Received packet, error %d, size %d, timestampValid %d: ", rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet.data(), rcvResult.size);
            } else print_dbg("[RTT] No packet received, timeout reached\n");
        }
    }
    
    if (success) {
        auto replyTime = rcvResult.timestamp + replyDelay;
        if (ENABLE_ROUNDTRIP_INFO_DBG)
            print_dbg("[T/R] ta=%lld, tr=%lld\n", rcvResult.timestamp, replyTime);
        ctx.configureTransceiver(ctx.getTransceiverConfig(false));
        LedBar<replyPacketSize> p(timesync->getDelayToMaster() / accuracy);
        ctx.sendAt(p.getPacket(), p.getPacketSize(), replyTime);
    } else if (ENABLE_ROUNDTRIP_INFO_DBG) {
        print_dbg("[T/R] tr=null\n");
    }
    
    ctx.transceiverIdle(); */
}
}


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

#include "../../timesync/led_bar.h"
#include "../debug_settings.h"
#include "listening_roundtrip.h"

namespace mxnet{

void ListeningRoundtripPhase::execute(long long slotStart) {
    //TODO add a way to use the syncStatus also with the master for having an optimized receiving window
    //maybe with a different class for the master node?
    long long timeoutTime = slotStart + receiverWindow + MediumAccessController::maxPropagationDelay + MediumAccessController::packetPreambleTime;
    
    bool success = false;
    for(; !(success || rcvResult.error == miosix::RecvResult::TIMEOUT); success = isRoundtripAskPacket()) {
        try {
            rcvResult = transceiver.recv(packet, replyPacketSize, timeoutTime);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
        if (ENABLE_PKT_INFO_DBG) {
            if(rcvResult.size){
                print_dbg("[RTT] Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
                if (ENABLE_PKT_DUMP_DBG)
                    memDump(packet, result.size);
            } else print_dbg("[RTT] No packet received, timeout reached\n");
        }
    }
    
    if (success) {
        auto replyTime = rcvResult.timestamp + replyDelay;
        if (ENABLE_ROUNDTRIP_INFO_DBG)
            print_dbg("[T/R] ta=%lld, tr=%lld\n", rcvResult.timestamp, replyTime);
        transceiver.configure(ctx.getTransceiverConfig(false));
        //TODO sto pacchetto non e` compatibile manco con se stesso, servono header di compatibilita`, indirizzo, etc etc
        LedBar<replyPacketSize> p;
        p.encode(ctx.getDelayToMaster() / accuracy); //TODO: 7?! should put a significant cumulated RTT here.
        try {
            transceiver.sendAt(p.getPacket(), p.getPacketSize(), replyTime);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
    } else if (ENABLE_ROUNDTRIP_INFO_DBG) {
        print_dbg("[T/R] tr=null\n");
    }
    
    transceiver.turnOff();
}
}


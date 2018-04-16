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

#include <stdio.h>

#include "led_bar.h"
#include "../../debug_settings.h"
#include "asking_roundtrip.h"
#include "listening_roundtrip.h"

using namespace miosix;

namespace mxnet {

    void AskingRoundtripPhase::execute(long long slotStart) {
        //Sending led bar request to the previous hop
        //Transceiver configured with non strict timeout
        //TODO deepsleep missing
        getRoundtripAskPacket();
        try {
            transceiver.sendAt(packet.data(), askPacketSize, slotStart);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
        if (ENABLE_ROUNDTRIP_INFO_DBG)
            print_dbg("[T/R] Asked Roundtrip\n");

        //Expecting a ledbar reply from any node of the previous hop, crc disabled
        transceiver.configure(ctx.getTransceiverConfig(false));
        bool success = false;
        for (; !(success || rcvResult.error == miosix::RecvResult::TIMEOUT); success = isRoundtripPacket()) {
            try {
                rcvResult = transceiver.recv(packet.data(), packet.size(), slotStart + replyDelay +
                        (MediumAccessController::maxPropagationDelay << 1) + tAskPkt +
                        MediumAccessController::packetPreambleTime + receiverWindow
                );
            } catch(std::exception& e) {
                if (ENABLE_RADIO_EXCEPTION_DBG)
                    print_dbg("%s\n", e.what());
            }
            if (ENABLE_PKT_INFO_DBG) {
                if(rcvResult.error != RecvResult::UNINITIALIZED){
                    print_dbg("Received packet, error %d, size %d, timestampValid %d: ", rcvResult.error, rcvResult.size, rcvResult.timestampValid);
                    if (ENABLE_PKT_DUMP_DBG)
                        memDump(packet.data(), rcvResult.size);
                } else print_dbg("No packet received, timeout reached\n");
            }
        }
        transceiver.turnOff();

        LedBar<replyPacketSize> p(packet.data());
        if(rcvResult.size == p.getPacketSize() && rcvResult.error == miosix::RecvResult::OK && rcvResult.timestampValid) {
            auto prevDelay = p.decode().first * accuracy;
            auto hopDelay = (rcvResult.timestamp - (slotStart + replyDelay)) >> 1; //time at which is received - time at which is sent / 2
            delayToMaster = prevDelay + hopDelay;
            if (ENABLE_ROUNDTRIP_INFO_DBG)
                print_dbg("[T/R] d=%lld d_h=%lld\n", prevDelay, hopDelay);
        } else if (ENABLE_ROUNDTRIP_INFO_DBG) {
            print_dbg("No roundtrip reply received\n");
        }
    }
}


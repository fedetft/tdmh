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

#include "askingroundtripphase.h"
#include "listeningroundtripphase.h"
#include "led_bar.h"
#include <stdio.h>

namespace miosix {
    AskingRoundtripPhase::~AskingRoundtripPhase() {
    }

    void AskingRoundtripPhase::execute(MACContext& ctx) {
        //Sending led bar request to the previous hop
        //Transceiver configured with non strict timeout
        greenLed::high();
        transceiver.configure(*ctx.getTransceiverConfig());
        transceiver.turnOn();
        //TODO deepsleep missing
        try {
            transceiver.sendAt(
                getRoundtripAskPacket(ctx.getMediumAccessController().getPanId()).data(), askPacketSize, globalFirstActivityTime);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_ROUNDTRIP_INFO_DBG
        printf("Asked Roundtrip\n");
#endif /* ENABLE_ROUNDTRIP_INFO_DBG */

        //Expecting a ledbar reply from any node of the previous hop, crc disabled
        auto* tc = ctx.getTransceiverConfig();
        transceiver.configure(TransceiverConfiguration(tc->frequency, tc->txPower, false, false));
        LedBar<replyPacketSize> p;
        RecvResult result;
        //TODO missing a cycle for skipping bad pkts
        try {
            result = transceiver.recv(p.getPacket(), p.getPacketSize(), globalFirstActivityTime + replyDelay);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            puts(e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_PKT_INFO_DBG
        if(result.error != RecvResult::ErrorCode::UNINITIALIZED){
            printf("Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
#ifdef ENABLE_PKT_DUMP_DBG
            memDump(p.getPacket(), result.size);
#endif /* ENABLE_PKT_DUMP_DBG */
        } else printf("No packet received, timeout reached\n");
#endif /* ENABLE_PKT_INFO_DBG */
        transceiver.turnOff();
        greenLed::low();

        if(result.size == p.getPacketSize() && result.error == RecvResult::ErrorCode::OK && result.timestampValid) {
            lastDelay = result.timestamp - (globalFirstActivityTime + replyDelay);
            totalDelay = p.decode().first * accuracy + lastDelay;
#ifdef ENABLE_ROUNDTRIP_INFO_DBG
            printf("delay=%lld total=%lld\n", lastDelay, totalDelay);
        } else {
            printf("No roundtrip reply received\n");
#endif /* ENABLE_ROUNDTRIP_INFO_DBG */
        }
    }
}


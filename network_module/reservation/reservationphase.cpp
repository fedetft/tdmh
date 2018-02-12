/***************************************************************************
 *   Copyright (C)  2018 by Polidori Paolo                                 *
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

#include "reservationphase.h"
#include "../flooding/syncstatus.h"

namespace miosix {
    ReservationPhase::~ReservationPhase() {
    }

    void ReservationPhase::receiveOrGetEmpty(MACContext& ctx) {
        auto* syncStatus = ctx.getSyncStatus();
        auto wakeUpTimeout = syncStatus->getWakeupAndTimeout(localFirstActivityTime);
        auto panId = ctx.getNetworkConfig()->panId;
        auto hop = ctx.getHop();
        auto now = getTime();
        if(now >= localFirstActivityTime - syncStatus->receiverWindow)
            printf("RP start late\n");
        if (now < wakeUpTimeout.first)
            pm.deepSleepUntil(wakeUpTimeout.first);
        ledOn();
        RecvResult result;
        for(bool success = false; !(success || result.error == RecvResult::ErrorCode::TIMEOUT);
                success = isReservationPacket(result, panId, hop))
        {
            try {
                result = transceiver.recv(packet.data(), ReservationPhase::reservationPacketSize, wakeUpTimeout.second );
            } catch(std::exception& e) {
    #ifdef ENABLE_RADIO_EXCEPTION_DBG
                printf("%s\n", e.what());
    #endif /* ENABLE_RADIO_EXCEPTION_DBG */
            }
    #ifdef ENABLE_PKT_INFO_DBG
            if(result.size){
                printf("[RTT] Received packet, error %d, size %d, timestampValid %d: ", result.error, result.size, result.timestampValid);
    #ifdef ENABLE_PKT_DUMP_DBG
                memDump(packet, result.size);
    #endif /* ENABLE_PKT_DUMP_DBG */
            } else printf("[RTT] No packet received, timeout reached\n");
    #endif /* ENABLE_PKT_INFO_DBG */
        }
        measuredActivityTime = result.timestamp;
        if(result.error != RecvResult::ErrorCode::OK) {//dead children, i desynchronize or just a leaf
            getEmptyPkt(panId, hop);
            measuredActivityTime = localFirstActivityTime;
        }
    }
    void ReservationPhase::populatePacket(MACContext& ctx) {
        ctx.getSlotsNegotiator();
        //TODO make the slots negotiator appropriately populate the pkt
        /*
        if(packet[6 + ctx.networkId])
            printf("[MAC] Reservation slot already occupied\n");
        packet[2]--;
        packet[6 + ctx.networkId] |= ctx.getMediumAccessController().getOutgoingCount() ? 255 : 0;*/
    }

    void ReservationPhase::forwardPacket() {
        //Sending synchronization start packet
        //TODO what's better, a causal consistency or a previously measured time?
        long long sendTime = measuredActivityTime + ReservationPhase::retransmissionDelay;
        try {
            transceiver.sendAt(packet.data(), sizeof(packet), sendTime);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_FLOODING_INFO_DBG
        printf("RP sent at %lld\n", sendTime);
#endif /* ENABLE_FLOODING_INFO_DBG */
    }



}

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

#pragma once

#include "roundtrip_subphase.h"
#include "../../../mac_context.h"

namespace mxnet {
class AskingRoundtripPhase : public RoundtripSubphase {
public:
    AskingRoundtripPhase(MACContext& ctx) :
        RoundtripSubphase(ctx) {};
    AskingRoundtripPhase() = delete;
    AskingRoundtripPhase(const AskingRoundtripPhase& orig) = delete;
    virtual ~AskingRoundtripPhase() {};
    void execute(long long slotStart) override;
    /*  long long getDelayToMaster() const { return delayToMaster; }
protected:
    void getRoundtripAskPacket() {
        auto panId = ctx.getNetworkConfig().getPanId();
        packet = {{
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            static_cast<unsigned char>(ctx.getHop() - 1), //seq no reused as glossy hop count, 0=root node, it has to contain the dest hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff                                //destination addr (broadcast)
        }};
    }
private:
    bool isRoundtripPacket() {
        auto panId = ctx.getNetworkConfig().getPanId();
        //TODO fix this, actually the packet doesn't have this format
        if (!(rcvResult.error == miosix::RecvResult::OK && rcvResult.timestampValid
                && rcvResult.size == replyPacketSize
                && packet[0] == 0x46 && packet[1] == 0x08
                && packet[2] == ctx.getHop()
                && packet[3] == static_cast<unsigned char>(panId >> 8)
                && packet[4] == static_cast<unsigned char>(panId & 0xff)
                && packet[5] == 0xff && packet[6] == 0xff)) return false;
        bool legit = true;
        for (int i = 0; i < replyPacketSize && legit; i++) {
            auto upper = packet[i] & 0xF0;
            auto lower = packet[i] & 0x0F;
            legit = upper == 0xF0 || upper == 0x00 || lower == 0x00 || lower == 0x0F;
        }
        return legit;
    }
    long long delayToMaster;*/
};
}



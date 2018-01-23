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

#include "masterfloodingphase.h"
#include "../maccontext.h"
#include "../macround/macround.h"
#include <stdio.h>

namespace miosix {

    MasterFloodingPhase::~MasterFloodingPhase() {
    }

    void MasterFloodingPhase::execute(MACContext& ctx)
    {
        auto sendTime = startTime + rootNodeWakeupAdvance;
        ledOn();
        transceiver.configure(*ctx.getTransceiverConfig());
        const unsigned char syncPacket[]=
        {
            0x46, //frame type 0b110 (reserved), intra pan
            0x08, //no source addressing, short destination addressing
            0x00, //seq no reused as glossy hop count, 0=root node, it has to contain the source hop
            static_cast<unsigned char>(panId>>8),
            static_cast<unsigned char>(panId & 0xff), //destination pan ID
            0xff, 0xff                                //destination addr (broadcast)
        };
        pm.deepSleepUntil(startTime);
        transceiver.turnOn();
        //Sending synchronization start packet
        try {
            transceiver.sendAt(syncPacket, sizeof(syncPacket), sendTime);
        } catch(std::exception& e) {
#ifdef ENABLE_RADIO_EXCEPTION_DBG
            printf("%s\n", e.what());
#endif /* ENABLE_RADIO_EXCEPTION_DBG */
        }
#ifdef ENABLE_FLOODING_INFO_DBG
        printf("Sync packet sent at %lld\n", sendTime);
#endif /* ENABLE_FLOODING_INFO_DBG */
        transceiver.turnOff();
        ledOff();
    }
}
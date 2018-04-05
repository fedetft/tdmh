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

#include "../debug_settings.h"
#include "master_timesync_downlink.h"
#include "../mac_context.h"

namespace mxnet {

    MasterTimesyncDownlink::~MasterTimesyncDownlink() {
    }

    void MasterTimesyncDownlink::execute(long long slotStart)
    {
        transceiver.configure(ctx.getTransceiverConfig());
        transceiver.turnOn();
        //Thread::nanoSleepUntil(startTime);
        auto deepsleepDeadline = syncStatus->getSenderWakeup(slotStart);
        if(getTime() < deepsleepDeadline)
            pm.deepSleepUntil(deepsleepDeadline);
        //Sending synchronization start packet
        try {
            transceiver.sendAt(getSyncPkt(networkConfig->panId).data(), syncPacketSize, slotStart);
        } catch(std::exception& e) {
            if (ENABLE_RADIO_EXCEPTION_DBG)
                print_dbg("%s\n", e.what());
        }
        transceiver.idle();
        if (ENABLE_TIMESYNC_DL_INFO_DBG)
            print_dbg("[T] st=%lld\n", slotStart);
        if (false)
            listeningRTP.execute(slotStart + RoundtripSubphase::senderDelay);
        transceiver.turnOff();
    }

}

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

#include "schedule_computation.h"
#include "../debug_settings.h"
#include "master_schedule_distribution.h"
#include "schedule_distribution.h"
#include "schedule_information.h"
#include "../timesync/timesync_downlink.h"
#include <algorithm>
#include <limits>

using namespace miosix;

namespace mxnet {
    void MasterScheduleDownlinkPhase::execute(long long slotStart) {
        getCurrentSchedule();
        sendSchedulePkt(slotStart);
    }

    void MasterScheduleDownlinkPhase::getCurrentSchedule() {
        currentSchedule = schedule_comp.getSchedule();
    }

    void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
        // Add header to packet

        // Add Schedule element

        // Begin radio sequence
        ctx.configureTransceiver(ctx.getTransceiverConfig());
        auto wakeUp = slotStart - MediumAccessController::sendingNodeWakeupAdvance;
        if(getTime() < wakeUp)
            ctx.sleepUntil(wakeUp);
        ctx.sendAt(packet.data(), MediumAccessController::maxPktSize, slotStart);
        if (ENABLE_SCHEDULE_DL_INFO_DBG)
            print_dbg("[SC] ST=%lld\n", slotStart);
        ctx.transceiverIdle();
    }

}


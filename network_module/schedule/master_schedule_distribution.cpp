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
#include "master_schedule_distribution.h"
#include "schedule_distribution.h"
#include "../timesync/timesync_downlink.h"
#include "../packet.h"
#include <algorithm>
#include <limits>

using namespace miosix;

namespace mxnet {

void MasterScheduleDownlinkPhase::execute(long long slotStart) {
    // Getting initial schedule
    if(header.getScheduleID() == 0)
        getCurrentSchedule();
    // Prepare to send new packet
    header.incrementPacketCounter();
    if(header.getPacketCounter() >= header.getTotalPacket()) {
        header.incrementRepetition();
        // TODO make maxScheduleRepetitions configurable
        if(header.getRepetition() >= 3) {
            header.resetRepetition();
            beginCountdown = true;
        }
    }
    sendSchedulePkt(slotStart);
}

void MasterScheduleDownlinkPhase::getCurrentSchedule() {
    currentSchedule = schedule_comp.getSchedule();
    // Build a header for the new schedule
    unsigned long oldID = header.getScheduleID();
    ScheduleHeader newheader(
        currentSchedule.size(), // totalPacket
        0,                      // currentPacket
        oldID + 1);             // scheduleID
    header = newheader;
}

void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
    Packet pkt;
    // Add schedule distribution header
    header.serialize(pkt);
    // Add schedule element to packet
    currentSchedule[header.getPacketCounter()].serialize(pkt);
    // Send schedule element packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

}


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
#include "../packet.h"

using namespace miosix;

namespace mxnet {

MasterScheduleDownlinkPhase::MasterScheduleDownlinkPhase(MACContext& ctx, ScheduleComputation& sch) :
        ScheduleDownlinkPhase(ctx),schedule_comp(sch) {
    // Get number of downlink slots
    unsigned tile_size = ctx.getSlotsInTileCount();
    unsigned dataslots_downlinktile = ctx.getDataSlotsInDownlinkTileCount();
    downlink_slots = tile_size - dataslots_downlinktile;
}

void MasterScheduleDownlinkPhase::execute(long long slotStart) {
    // Check for new schedule
    if(schedule_comp.getScheduleID() != header.getScheduleID())
        getCurrentSchedule();
    // Prepare to send new packet
    if(header.getScheduleID() == 0)
        print_dbg("[D] no schedule to send\n");
    else {
        header.incrementPacketCounter();
        if(header.getPacketCounter() > header.getTotalPacket()) {
            header.resetPacketCounter();
            header.incrementRepetition();
            // TODO make maxScheduleRepetitions configurable
            if(header.getRepetition() >= 3) {
                header.resetRepetition();
                beginCountdown = true;
            }
        }
        print_dbg("[D] sending schedule %u/%u/%lu/%d/%d\n",
               header.getTotalPacket(),
               header.getPacketCounter(),
               header.getScheduleID(),
               header.getRepetition(),
               header.getCountdown());
        // To avoid caching of stdout
        fflush(stdout);
        sendSchedulePkt(slotStart);
    }
}

void MasterScheduleDownlinkPhase::getCurrentSchedule() {
    currentSchedule = schedule_comp.getSchedule();
    // Build a header for the new schedule
    ScheduleHeader newheader(
        currentSchedule.size(), // totalPacket
        0,                      // currentPacket
        schedule_comp.getScheduleID()); // scheduleID
    header = newheader;
}

void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
    Packet pkt;
    // Add schedule distribution header
    header.serialize(pkt);
    for(unsigned int i = 0; (i < downlink_slots) && (position < currentSchedule.size()); i++) {
        // Add schedule element to packet
        currentSchedule[position].serialize(pkt);
        position++;
    }
    // Send schedule downlink packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

}


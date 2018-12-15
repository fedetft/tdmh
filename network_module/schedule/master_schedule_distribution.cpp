/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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
#include "../tdmh.h"
#include "../packet.h"
#include "../debug_settings.h"

using namespace miosix;

namespace mxnet {

MasterScheduleDownlinkPhase::MasterScheduleDownlinkPhase(MACContext& ctx, ScheduleComputation& sch) :
        ScheduleDownlinkPhase(ctx),schedule_comp(sch) {
    // Get number of downlink slots
    unsigned tileSize = ctx.getSlotsInTileCount();
    unsigned dataslotsDownlinktile = ctx.getDataSlotsInDownlinkTileCount();
    downlinkSlots = tileSize - dataslotsDownlinktile;
    unsigned pktSize = MediumAccessController::maxPktSize;
    packetCapacity = (pktSize - sizeof(ScheduleHeaderPkt)) / sizeof(ScheduleElementPkt);
}

void MasterScheduleDownlinkPhase::execute(long long slotStart) {
    // Check for new schedule
    if(schedule_comp.getScheduleID() != header.getScheduleID()) { 
        getCurrentSchedule();
        beginCountdown = false;
    }
    // Send new schedule mode
    if(beginCountdown == false) {
        if(header.getScheduleID() == 0) {
            print_dbg("[D] no schedule to send\n");
            return;
        }
        if(header.getPacketCounter() > header.getTotalPacket()) {
            header.resetPacketCounter();
            header.incrementRepetition();
        }
        // repetition is a 2 bit value {0,3},
        // overflow is prevented in ScheduleHeader::incrementRepetition()
        if(header.getRepetition() == 3){ 
            beginCountdown = true;
            prepareCountdownHeader();
        }

        print_dbg("[D] sending schedule %u/%u/%lu/%d/%d\n",
                  header.getTotalPacket(),
                  header.getPacketCounter(),
                  header.getScheduleID(),
                  header.getRepetition(),
                  header.getCountdown());

        sendSchedulePkt(slotStart);
        header.incrementPacketCounter();
    }
    // Countdown mode
    else {
        if(countdownHeader.getCountdown() == 0)
            {
                // If countdown ended, wait for new schedule.
                return;
            }
        else{
            print_dbg("[D] sending schedule %u/%u/%lu/%d/%d\n",
                      countdownHeader.getTotalPacket(),
                      countdownHeader.getPacketCounter(),
                      countdownHeader.getScheduleID(),
                      countdownHeader.getRepetition(),
                      countdownHeader.getCountdown());
            sendCountdownPkt(slotStart);
            countdownHeader.decrementCountdown();
        }
    }
}

void MasterScheduleDownlinkPhase::getCurrentSchedule() {
    currentSchedule = schedule_comp.getSchedule();
    // Build a header for the new schedule
    unsigned numPackets = (currentSchedule.size() / packetCapacity) + 1;
    ScheduleHeader newheader(
        numPackets,                     // totalPacket
        1,                              // currentPacket
        schedule_comp.getScheduleID()); // scheduleID
    header = newheader;
}

void MasterScheduleDownlinkPhase::prepareCountdownHeader() {
    ScheduleHeader cdHeader(
                            0,                        // totalPacket
                            0,                        // currentPacket
                            header.getScheduleID(),   // scheduleID
                            0);                       // repetition
    countdownHeader = cdHeader;
}


void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
    Packet pkt;
    // Add schedule distribution header
    header.serialize(pkt);
    for(unsigned int i = 0; (i < packetCapacity) && (position < currentSchedule.size()); i++) {
        // Add schedule element to packet
        currentSchedule[position].serialize(pkt);
        position++;
    }
    // Send schedule downlink packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

void MasterScheduleDownlinkPhase::sendCountdownPkt(long long slotStart) {
    Packet pkt;
    // Add schedule countdown header
    countdownHeader.serialize(pkt);
    // Send schedule countdown packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

}


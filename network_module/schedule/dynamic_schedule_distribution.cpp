/***************************************************************************
 *   Copyright (C)  2017 by Federico Amedeo Izzo                           *
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

#include "dynamic_schedule_distribution.h"
#include "../tdmh.h"
#include "../packet.h"
#include "../debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicScheduleDownlinkPhase::execute(long long slotStart) {
    if(replaceCountdown != 0)
        replaceCountdown--;
    Packet pkt;
    // Receive the schedule packet
    auto arrivalTime = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, arrivalTime);
    ctx.transceiverIdle(); //Save power waiting for rebroadcast time
    
    if (rcvResult.error == miosix::RecvResult::TIMEOUT) return;

    // Rebroadcast the schedule packet
    if(ctx.getHop() >= ctx.getNetworkConfig().getMaxHops()) return;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, rcvResult.timestamp + rebroadcastInterval);
    ctx.transceiverIdle();

    // Parse the schedule packet
    SchedulePacket spkt = SchedulePacket::deserialize(pkt);
    decodePacket(spkt);

    auto header = spkt.getHeader();
    printHeader(header);
    calculateCountdown(header);
    // Check replaceCountdown
    if(replaceCountdown == 1) { 
        replaceRunningSchedule();
        if(explicitScheduleID != header.getScheduleID()) {
            printSchedule();
            expandSchedule();
            explicitScheduleID = header.getScheduleID();
            printExplicitSchedule();
        }
        checkTimeSetSchedule();
    }

    //printStatus();
}

void DynamicScheduleDownlinkPhase::decodePacket(SchedulePacket& spkt) {
    ScheduleHeader newHeader = spkt.getHeader();
    auto myID = ctx.getNetworkId();
    // We received a new schedule, replace currently received
    if((newHeader.getScheduleID() != nextHeader.getScheduleID())) {
        printf("Node:%d New schedule received!\n", myID);
        nextHeader = newHeader;
        nextSchedule = spkt.getElements();
        // Resize the received bool vector to the size of the new schedule
        received.clear();
        received.resize(newHeader.getTotalPacket(), false);
        // Set current packet as received
        received.at(newHeader.getCurrentPacket()) = true;
        // Reset the schedule replacement countdown;
        replaceCountdown = 0;
    }
    // We are receiving another part of the same schedule, we accept it if:
    // - repetition number is higher or equal than the saved one
    // - packet has not been already received (avoid duplicates)
    else if((newHeader.getScheduleID() == nextHeader.getScheduleID()) &&
            (newHeader.getRepetition() >= nextHeader.getRepetition()) &&
            (!received.at(newHeader.getCurrentPacket()))) {
        printf("Node:%d Piece %d of schedule received!\n", myID, newHeader.getCurrentPacket());
        // Set current packet as received
        received.at(newHeader.getCurrentPacket()) = true;
        nextHeader = newHeader;
        // Add elements from received packet to new schedule
        nextSchedule.insert(nextSchedule.begin(), spkt.getElements().begin(), spkt.getElements().end());
    }
    // If we receive an header with ID less of what we have, discard it
}

void DynamicScheduleDownlinkPhase::printHeader(ScheduleHeader& header) {
    printf("[D] node %d, hop %d, received schedule %u/%u/%lu/%d\n",
              ctx.getNetworkId(),
              ctx.getHop(),
              header.getTotalPacket(),
              header.getCurrentPacket(),
              header.getScheduleID(),
              header.getRepetition());
}

void DynamicScheduleDownlinkPhase::calculateCountdown(ScheduleHeader& newHeader) {
    // If we received a complete schedule, calculate activation time
    bool complete = true;
    for(int i=0; i < received.size(); i++) complete &= received[i];
    if(complete) {
        // Becomes 1 with 3rd repetition (1 = replace schedule)
        replaceCountdown = 4 - newHeader.getRepetition();
    }
}

void DynamicScheduleDownlinkPhase::replaceRunningSchedule() {
    printf("Countdown: %d, replacing old schedule\n", replaceCountdown);
    header = nextHeader;
    schedule = nextSchedule;
}

void DynamicScheduleDownlinkPhase::printStatus() {
    print_dbg("[D] node:%d, countdown:%d, received:%d[",
           ctx.getNetworkId(),
           replaceCountdown,
           received.size());
    for (int i=0; i < received.size(); i++) printf("%d", static_cast<bool>(received[i]));
    printf("]\n");
}

}


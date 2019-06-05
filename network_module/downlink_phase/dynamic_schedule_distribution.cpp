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
#include "../util/packet.h"
#include "../util/debug_settings.h"

using namespace miosix;

namespace mxnet {

void DynamicScheduleDownlinkPhase::execute(long long slotStart) {
    Packet pkt;
    // Receive the schedule packet
    auto arrivalTime = slotStart + (ctx.getHop() - 1) * rebroadcastInterval;
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    auto rcvResult = pkt.recv(ctx, arrivalTime);
    ctx.transceiverIdle(); //Save power waiting for rebroadcast time
    // No schedule received
    if (rcvResult.error != RecvResult::ErrorCode::OK || pkt.checkPanHeader(panId) == false) {
        if(replaceCountdown != 5 && replaceCountdown != 0)
            replaceCountdown--;
    }
    // Schedule received
    else {
        // Rebroadcast the schedule packet
        if(ctx.getHop() >= ctx.getNetworkConfig().getMaxHops()) return;
        ctx.configureTransceiver(ctx.getTransceiverConfig());
        pkt.send(ctx, rcvResult.timestamp + rebroadcastInterval);
        ctx.transceiverIdle();

        // Parse the schedule packet
        ScheduleHeader newHeader = decodePacket(pkt);

        if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            printHeader(newHeader);
        // If we received a complete schedule, calculate activation time
        if(isScheduleComplete())
            calculateCountdown(newHeader);
    }
    // Check replaceCountdown
    if(isScheduleComplete() &&
       (replaceCountdown == 0) &&
       (nextHeader.getScheduleID() != header.getScheduleID())) {
        replaceRunningSchedule();
        if(explicitScheduleID != header.getScheduleID()) {
            auto myID = ctx.getNetworkId();
            if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
                printSchedule(myID);
            std::vector<ExplicitScheduleElement> NewExplicitSchedule = expandSchedule(myID);
            explicitSchedule = std::move(NewExplicitSchedule);
            explicitScheduleID = header.getScheduleID();
            if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG) {
                print_dbg("[SD] Explicit schedule:\n");
                printExplicitSchedule(myID, true, explicitSchedule);                
            }
        }
    }
    if(isScheduleComplete() && (nextHeader.getScheduleID() == header.getScheduleID())) {
        // The check on schedule activation must be done only after receiving the
        // first schedule (copied from nextSchedule in replaceRunningSchedule())
        checkTimeSetSchedule(slotStart);
    }
    //printStatus();
}

ScheduleHeader DynamicScheduleDownlinkPhase::decodePacket(Packet& pkt) {
    SchedulePacket spkt = SchedulePacket::deserialize(pkt);
    ScheduleHeader newHeader = spkt.getHeader();
    std::vector<ScheduleElement> elements = spkt.getElements();
    unsigned int numPackets = newHeader.getTotalPacket();
    // Received Info Packet
    if(numPackets == 0){
        nextInfos = std::vector<InfoElement>(elements.begin(), elements.end());
    }
    // Received Schedule + Info packet
    else {
        auto myID = ctx.getNetworkId();
        // Check for Info Elements and separate them from Schedule Elements
        auto firstInfo = std::find_if(elements.begin(), elements.end(), [](ScheduleElement s){
                         return (s.getTx()==0 && s.getRx()==0); });
        if(firstInfo != elements.end()) {
            nextInfos = std::vector<InfoElement>(firstInfo, elements.end());
            elements.erase(firstInfo, elements.end());
        }
        // We received a new schedule, replace currently received
        if((newHeader.getScheduleID() != nextHeader.getScheduleID())) {
            if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
                print_dbg("[SD] Node:%d New schedule received!\n", myID);
            nextHeader = newHeader;
            nextSchedule = elements;
            // Resize the received bool vector to the size of the new schedule
            received.clear();
            received.resize(newHeader.getTotalPacket(), false);
            // Set current packet as received
            received.at(newHeader.getCurrentPacket()) = true;
            // Reset the schedule replacement countdown;
            replaceCountdown = 5;
        }
        // We are receiving another part of the same schedule, we accept it if:
        // - repetition number is higher or equal than the saved one
        // - packet has not been already received (avoid duplicates)
        else if((newHeader.getScheduleID() == nextHeader.getScheduleID()) &&
                (newHeader.getRepetition() >= nextHeader.getRepetition()) &&
                (!received.at(newHeader.getCurrentPacket()))) {
            if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
                print_dbg("[SD] Node:%d Piece %d of schedule received!\n", myID, newHeader.getCurrentPacket());
            // Set current packet as received
            received.at(newHeader.getCurrentPacket()) = true;
            nextHeader = newHeader;
            // Add elements from received packet to new schedule
            nextSchedule.insert(nextSchedule.begin(), elements.begin(), elements.end());
        }
        // If we receive an header with ID less of what we have, discard it
    }
    return newHeader;
}

void DynamicScheduleDownlinkPhase::printHeader(ScheduleHeader& header) {
    print_dbg("[SD] node %d, hop %d, received schedule %u/%u/%lu/%d\n",
              ctx.getNetworkId(),
              ctx.getHop(),
              header.getTotalPacket(),
              header.getCurrentPacket(),
              header.getScheduleID(),
              header.getRepetition());
}

void DynamicScheduleDownlinkPhase::calculateCountdown(ScheduleHeader& newHeader) {
    // Becomes 1 with 3rd repetition (0 = replace schedule)
    replaceCountdown = 4 - newHeader.getRepetition();
}

bool DynamicScheduleDownlinkPhase::isScheduleComplete() {
    // If no packet was received, the schedule is not complete
    if(received.size() == 0)
        return false;
    bool complete = true;
    for(unsigned int i=0; i< received.size(); i++) complete &= received[i];
    return complete;
}

void DynamicScheduleDownlinkPhase::replaceRunningSchedule() {
    header = nextHeader;
    schedule = nextSchedule;
    infos = nextInfos;
    // Apply info elements to StreamManager
    // NOTE: apply info element here because we received 3 times the schedule
    // If we don't apply them, they can be lost or remain in the queue
    streamMgr->applyInfoElements(infos);
}

void DynamicScheduleDownlinkPhase::printStatus() {
    print_dbg("[SD] node:%d, countdown:%d, received:%d[",
           ctx.getNetworkId(),
           replaceCountdown,
           received.size());
    for (unsigned int i=0; i < received.size(); i++) print_dbg("%d", static_cast<bool>(received[i]));
    print_dbg("]\n");
}

}


/***************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo                       *
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
    // Received a valid schedule packet
    if (rcvResult.error == RecvResult::ErrorCode::OK && pkt.checkPanHeader(panId) == true) {
        // Retransmit the schedule packet unless you belong to maximum hop
        if(ctx.getHop() < ctx.getNetworkConfig().getMaxHops()) {
            ctx.configureTransceiver(ctx.getTransceiverConfig());
            pkt.send(ctx, rcvResult.timestamp + rebroadcastInterval);
            ctx.transceiverIdle();
        }
        // Parse the schedule packet
        SchedulePacket spkt = SchedulePacket::deserialize(pkt, panId);
        ScheduleHeader newHeader = spkt.getHeader();
        if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            printHeader(newHeader);
        // Received Info Packet (no ScheduleElements)
        if(newHeader.getTotalPacket() == 0){
            std::vector<ScheduleElement> elements = spkt.getElements();
            infos = std::vector<InfoElement>(elements.begin(), elements.end());
        }
        // Received Schedule + Info packet
        else {
            // Extract and delete info elements from packet
            extractInfoElements(spkt);
            // We received a new schedule, replace currently received
            if((newHeader.getScheduleID() != header.getScheduleID())) {
                if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
                    print_dbg("[SD] Node:%d New schedule received!\n", myId);
                // Resize the received bool vector to the size of the new schedule
                received.clear();
                received.resize(newHeader.getTotalPacket(), false);
                // Set current packet as received
                received.at(newHeader.getCurrentPacket()) = true;
                // Replace old schedule header and elements
                header = newHeader;
                schedule = spkt.getElements();
            }
            // We are receiving another part of the same schedule, we accept it if:
            // - schedule ID is equal to the saved one (else block)
            // - packet has not been already received (avoid duplicates)
            else if(!received.at(newHeader.getCurrentPacket())) {
                if(ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
                    print_dbg("[SD] Node:%d Piece %d of schedule received!\n", myId, newHeader.getCurrentPacket());
                // Set current packet as received
                received.at(newHeader.getCurrentPacket()) = true;
                // Replace old schedule header
                header = newHeader;
                // Add elements from received packet to new schedule
                std::vector<ScheduleElement> elements = spkt.getElements();
                schedule.insert(schedule.end(), elements.begin(), elements.end());
            }
        }
        // If we received info elements, apply them
        if(!infos.empty()) {            
            streamMgr->applyInfoElements(infos);
            infos.clear();
        }
    }
    // If we received a complete schedule, check application tile
    if(isScheduleComplete()) {
        checkTimeSetSchedule(slotStart);
    }
    //printStatus();
}

void DynamicScheduleDownlinkPhase::extractInfoElements(SchedulePacket& spkt) {
    std::vector<ScheduleElement> elements = spkt.getElements();
    // Check for Info Elements and separate them from Schedule Elements
    auto firstInfo = std::find_if(elements.begin(), elements.end(),
                                  [](ScheduleElement s){
                                      return (s.getTx()==0 && s.getRx()==0);
                                  });
    if(firstInfo != elements.end()) {
        infos = std::vector<InfoElement>(firstInfo, elements.end());
        spkt.popElements(infos.size());
    }
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

bool DynamicScheduleDownlinkPhase::isScheduleComplete() {
    // If no packet was received, the schedule is not complete
    if(received.size() == 0)
        return false;
    bool complete = true;
    for(unsigned int i=0; i< received.size(); i++) complete &= received[i];
    return complete;
}

void DynamicScheduleDownlinkPhase::printStatus() {
    print_dbg("[SD] node:%d, received:%d[",
           myId,
           received.size());
    for (unsigned int i=0; i < received.size(); i++) print_dbg("%d", static_cast<bool>(received[i]));
    print_dbg("]\n");
}

}

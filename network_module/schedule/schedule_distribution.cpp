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

#include "schedule_distribution.h"
#include "../timesync/networktime.h"
#include "../data/dataphase.h"

using namespace miosix;

namespace mxnet {

std::vector<ExplicitScheduleElement> ScheduleDownlinkPhase::expandSchedule(unsigned char nodeID) {
    // New explicitSchedule to return
    std::vector<ExplicitScheduleElement> result;
    // Resize new explicitSchedule and fill with default value (sleep)
    auto slotsInTile = ctx.getSlotsInTileCount();
    auto scheduleSlots = header.getScheduleTiles() * slotsInTile;
    result.resize(scheduleSlots, ExplicitScheduleElement());
    // Scan implicit schedule for element that imply the node action
    for(auto e : schedule) {
        // Send from stream case
        if(e.getSrc() == nodeID && e.getTx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(Action::SENDSTREAM, e.getStreamId()); 
            }
        }
        // Receive to stream case
        if(e.getDst() == nodeID && e.getRx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(Action::RECVSTREAM, e.getStreamId()); 
            }
        }
        // Send from buffer case (send saved multi-hop packet)
        if(e.getSrc() != nodeID && e.getTx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(Action::SENDBUFFER, e.getStreamId()); 
            }
        }
        // Receive to buffer case (receive and save multi-hop packet)
        if(e.getDst() != nodeID && e.getRx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(Action::RECVBUFFER, e.getStreamId()); 
            }
        }
    }
    return result;
}

void ScheduleDownlinkPhase::printSchedule(unsigned char nodeID) {
    printf("[SD] Node: %d, implicit schedule n. %lu\n", nodeID, header.getScheduleID());
    printf("ID   TX  RX  PER OFF\n");
    for(auto& elem : schedule) {
        printf("%d  %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
               toInt(elem.getPeriod()), elem.getOffset());
    }
}

    void ScheduleDownlinkPhase::printExplicitSchedule(unsigned char nodeID, bool printHeader, std::vector<ExplicitScheduleElement> expSchedule) {
    auto slotsInTile = ctx.getSlotsInTileCount();
    // print header
    if(printHeader) {
        printf("        | ");
        for(unsigned int i=0; i<expSchedule.size(); i++) {
            printf("%2d ", i);
            if(((i+1) % slotsInTile) == 0)
                printf("| ");
        }
        printf("\n");
    }
    // print schedule line
    printf("Node: %2d|", nodeID);
    for(unsigned int i=0; i<expSchedule.size(); i++)
        {
            switch(expSchedule[i].getAction()) {
            case Action::SLEEP:
                printf(" _ ");
                break;
            case Action::SENDSTREAM:
                printf(" SS");
                break;
            case Action::RECVSTREAM:
                printf(" RS");
                break;
            case Action::SENDBUFFER:
                printf(" SB");
                break;
            case Action::RECVBUFFER:
                printf(" RB");
                break;
            }
            if(((i+1) % slotsInTile) == 0)
                printf(" |");
        }
    printf("\n");
}

void ScheduleDownlinkPhase::printCompleteSchedule() {
    auto myID = ctx.getNetworkId();
    if(myID == 0)
        printf("[SD] ### Schedule distribution, Master node\n");
    auto maxNodes = ctx.getNetworkConfig().getMaxNodes();
    printSchedule(myID);
    printf("[SD] ### Explicit Schedule for all nodes\n");
    std::vector<ExplicitScheduleElement> nodeSchedule;
    for(unsigned char node = 0; node < maxNodes; node++)
    {
        nodeSchedule = expandSchedule(node);
        // Print schedule header only for first line
        printExplicitSchedule(node, (node == 0), nodeSchedule);
    } 
}

void ScheduleDownlinkPhase::checkTimeSetSchedule() {
    auto nt = NetworkTime::now();
    auto tileDuration = ctx.getNetworkConfig().getTileDuration();
    auto tilesPassedTotal = nt.get() / tileDuration;
    if (tilesPassedTotal == header.getActivationTile()) {
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG || ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            print_dbg("[SD] Activating schedule n.%2d", explicitScheduleID);
        dataPhase->setSchedule(explicitSchedule);
        dataPhase->setScheduleTiles(header.getScheduleTiles());
        dataPhase->setScheduleActivationTile(header.getActivationTile());
    }
}

}

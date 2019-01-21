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

void ScheduleDownlinkPhase::expandSchedule(unsigned char nodeID) {
    // Resize explicitSchedule and fill with default value (sleep)
    auto slotsInTile = ctx.getSlotsInTileCount();
    auto scheduleSlots = header.getScheduleTiles() * slotsInTile;
    explicitSchedule.clear();
    explicitSchedule.resize(scheduleSlots,
                            ExplicitScheduleElement(Action::SLEEP, 0));
    // Scan implicit schedule for element that imply the node action
    for(auto e : schedule) {
        // Send from stream case
        if(e.getSrc() == nodeID && e.getTx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::SENDSTREAM, e.getSrcPort()); 
            }
        }
        // Receive to stream case
        if(e.getDst() == nodeID && e.getRx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::RECVSTREAM, e.getDstPort()); 
            }
        }
        // Send from buffer case (send saved multi-hop packet)
        if(e.getSrc() != nodeID && e.getTx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::SENDBUFFER, 0); 
            }
        }
        // Receive to buffer case (receive and save multi-hop packet)
        if(e.getDst() != nodeID && e.getRx() == nodeID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::RECVBUFFER, 0); 
            }
        }
    }
}

void ScheduleDownlinkPhase::printSchedule(unsigned char nodeID) {
    printf("Node: %d, implicit schedule n. %d\n", nodeID, header.getScheduleID());
    printf("ID   TX  RX  PER OFF\n");
    for(auto& elem : schedule) {
        printf("%d  %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
               toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleDownlinkPhase::printExplicitSchedule(unsigned char nodeID) {
    auto slotsInTile = ctx.getSlotsInTileCount();
    printf("Node: %2d| ", nodeID);
    for(int i=0; i<explicitSchedule.size(); i++) {
        printf("%2d ", i);
        if(((i+1) % slotsInTile) == 0)
            printf("| ");
    }
    printf("\n");
    printExplicitScheduleLine();
}

void ScheduleDownlinkPhase::printExplicitScheduleLine() {
    auto slotsInTile = ctx.getSlotsInTileCount();
    printf("        |");
    for(int i=0; i<explicitSchedule.size(); i++)
    {
        switch(explicitSchedule[i].getAction()) {
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

void ScheduleDownlinkPhase::checkTimeSetSchedule() {
    auto nt = NetworkTime::now();
    auto tileDuration = ctx.getNetworkConfig().getTileDuration();
    auto tilesPassedTotal = nt.get() / tileDuration;
    if (tilesPassedTotal == header.getActivationTile()) {
        dataPhase->setSchedule(explicitSchedule);
        dataPhase->setScheduleTiles(header.getScheduleTiles());
        dataPhase->setScheduleActivationTile(header.getActivationTile());
    }
}

}

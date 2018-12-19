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

void ScheduleDownlinkPhase::expandSchedule() {
    auto myID = ctx.getNetworkId();
    // Resize explicitSchedule and fill with default value (sleep)
    auto slotsInTile = ctx.getSlotsInTileCount();
    printf("[D] ID:%d slotsInTile = %d\n", myID, slotsInTile);
    printf("[D] ID:%d scheduleTiles = %d\n", myID, header.getScheduleTiles());
    auto scheduleSlots = header.getScheduleTiles() * slotsInTile;
    printf("[D] ID:%d scheduleslots = %d\n", myID, scheduleSlots);
    explicitSchedule.clear();
    explicitSchedule.resize(scheduleSlots,
                            ExplicitScheduleElement(Action::SLEEP, 0));
    // Scan implicit schedule for element that imply the node action
    for(auto e : schedule) {
        // Send from stream case
        if(e.getSrc() == myID && e.getTx() == myID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::SENDSTREAM, e.getSrcPort()); 
            }
        }
        // Receive to stream case
        if(e.getDst() == myID && e.getRx() == myID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::RECVSTREAM, e.getDstPort()); 
            }
        }
        // Send from buffer case (send saved multi-hop packet)
        if(e.getSrc() != myID && e.getTx() == myID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::SENDBUFFER, 0); 
            }
        }
        // Receive to buffer case (receive and save multi-hop packet)
        if(e.getDst() != myID && e.getRx() == myID) {
            // Period is normally expressed in tiles, get period in slots
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                explicitSchedule[slot] = ExplicitScheduleElement(Action::RECVBUFFER, 0); 
            }
        }
    }
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

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
#include "timesync/networktime.h"
#include "../data_phase/dataphase.h"

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
        // Period is normally expressed in tiles, get period in slots
        auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
        Action action = Action::SLEEP;
        // Send from stream case
        if(e.getSrc() == nodeID && e.getTx() == nodeID)
            action = Action::SENDSTREAM;
        // Receive to stream case
        if(e.getDst() == nodeID && e.getRx() == nodeID)
            action = Action::RECVSTREAM;
        // Send from buffer case (send saved multi-hop packet)
        if(e.getSrc() != nodeID && e.getTx() == nodeID)
            action = Action::SENDBUFFER;
        // Receive to buffer case (receive and save multi-hop packet)
        if(e.getDst() != nodeID && e.getRx() == nodeID)
            action = Action::RECVBUFFER;
        // Apply action if different than SLEEP (to avoid overwriting already scheduled slots)
        if(action != Action::SLEEP) {
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(action, e.getStreamInfo()); 
            }
        }
    }
    return result;
}

void ScheduleDownlinkPhase::printSchedule(unsigned char nodeID) {
    print_dbg("[SD] Node: %d, implicit schedule n. %lu\n", nodeID, header.getScheduleID());
    print_dbg("ID   TX  RX  PER OFF\n");
    for(auto& elem : schedule) {
        print_dbg("%d  %d-->%d   %d   %d\n", elem.getKey(), elem.getTx(), elem.getRx(),
               toInt(elem.getPeriod()), elem.getOffset());
    }
}

void ScheduleDownlinkPhase::printExplicitSchedule(unsigned char nodeID, bool printHeader, std::vector<ExplicitScheduleElement> expSchedule) {
    auto slotsInTile = ctx.getSlotsInTileCount();
    if(explicitSchedule.size() > (2 * slotsInTile)) {
        print_dbg("[SD] Not printing schedule longer than 2 tiles\n");
        return;
    }
    // print header
    if(printHeader) {
        print_dbg("        | ");
        for(unsigned int i=0; i<expSchedule.size(); i++) {
            print_dbg("%2d ", i);
            if(((i+1) % slotsInTile) == 0)
                print_dbg("| ");
        }
        print_dbg("\n");
    }
    // print schedule line
    print_dbg("Node: %2d|", nodeID);
    for(unsigned int i=0; i<expSchedule.size(); i++)
        {
            switch(expSchedule[i].getAction()) {
            case Action::SLEEP:
                print_dbg(" _ ");
                break;
            case Action::SENDSTREAM:
                print_dbg(" SS");
                break;
            case Action::RECVSTREAM:
                print_dbg(" RS");
                break;
            case Action::SENDBUFFER:
                print_dbg(" SB");
                break;
            case Action::RECVBUFFER:
                print_dbg(" RB");
                break;
            }
            if(((i+1) % slotsInTile) == 0)
                print_dbg(" |");
        }
    print_dbg("\n");
}

void ScheduleDownlinkPhase::printCompleteSchedule() {
#ifdef _MIOSIX
    print_dbg("[SD] Warning!: printing complete schedule on real hardware delays the TDMH thread and can cause <too late to send> errors! \n");
#endif
    auto myID = ctx.getNetworkId();
    if(myID == 0)
        print_dbg("[SD] ### Schedule distribution, Master node\n");
    auto maxNodes = ctx.getNetworkConfig().getMaxNodes();
    printSchedule(myID);
    for(auto& elem : schedule) {
        if(toInt(elem.getPeriod()) > 2) {
            print_dbg("[SD] Not printing schedule longer than 2 tiles\n");
            return;
        }
    }
    print_dbg("[SD] ### Explicit Schedule for all nodes (maxnodes=%d)\n", maxNodes);
    std::vector<ExplicitScheduleElement> nodeSchedule;
    for(unsigned char node = 0; node < maxNodes; node++)
    {
        nodeSchedule = expandSchedule(node);
        // Print schedule header only for first line
        printExplicitSchedule(node, (node == 0), nodeSchedule);
    } 
}

void ScheduleDownlinkPhase::checkTimeSetSchedule(long long slotStart) {
    if(explicitScheduleID == dataPhase->getScheduleID())
        return;
    auto currentTile = ctx.getCurrentTile(slotStart);
    if (currentTile >= header.getActivationTile()) {
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG || ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            print_dbg("[SD] Activating schedule n.%2lu\n", explicitScheduleID);
        // Apply schedule to DataPhase
        dataPhase->setSchedule(explicitSchedule);
        dataPhase->setScheduleTiles(header.getScheduleTiles());
        dataPhase->setScheduleActivationTile(header.getActivationTile());
        dataPhase->setScheduleID(explicitScheduleID);
        // Apply schedule to StreamManager
        streamMgr->applySchedule(schedule);
        // Apply info elements to StreamManager
        streamMgr->applyInfoElements(infos);
        // NOTE: Setting this flag to false enables Schedule Computation
        distributing = false;
    }
    else{
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG || ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            print_dbg("[SD] Early schedule activation! current:%2lu activation:%2lu\n",
                      currentTile, header.getActivationTile());
    }
}

}

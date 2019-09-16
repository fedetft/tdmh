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

#include "schedule_distribution.h"
#include "timesync/networktime.h"
#include "../data_phase/dataphase.h"

using namespace miosix;

namespace mxnet {

std::vector<ExplicitScheduleElement> ScheduleDownlinkPhase::expandSchedule(unsigned char nodeID) {
    // New explicitSchedule to return
    std::vector<ExplicitScheduleElement> result;
    std::map<unsigned int,std::shared_ptr<Packet>> buffers;
    // Resize new explicitSchedule and fill with default value (sleep)
    auto slotsInTile = ctx.getSlotsInTileCount();
    auto scheduleSlots = header.getScheduleTiles() * slotsInTile;
    result.resize(scheduleSlots, ExplicitScheduleElement());
    // Scan implicit schedule for element that imply the node action
    for(auto e : schedule) {
        // Period is normally expressed in tiles, get period in slots
        auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
        Action action = Action::SLEEP;
        std::shared_ptr<Packet> buffer;
        // Send from stream case
        if(e.getSrc() == nodeID && e.getTx() == nodeID)
            action = Action::SENDSTREAM;
        // Receive to stream case
        else if(e.getDst() == nodeID && e.getRx() == nodeID)
            action = Action::RECVSTREAM;
        // Send from buffer case (send saved multi-hop packet)
        else if(e.getSrc() != nodeID && e.getTx() == nodeID) {
            action = Action::SENDBUFFER;
            auto it=buffers.find(e.getStreamId().getKey());
            if(it!=buffers.end())
            {
                buffer=it->second;
            } else {
                //Should never happen, how can we transmit from a buffer we haven't received from?
                print_dbg("Error: ScheduleDownlinkPhase::expandSchedule missing buffer\n");
            }
        // Receive to buffer case (receive and save multi-hop packet)
        } else if(e.getDst() != nodeID && e.getRx() == nodeID) {
            action = Action::RECVBUFFER;
            auto key=e.getStreamId().getKey();
            auto it=buffers.find(key);
            if(it!=buffers.end())
            {
                //May happen because of redundancy, in this case we'll happily share the buffer
                buffer=it->second;
            } else {
                buffer=std::shared_ptr<Packet>(new Packet);
                buffers[key]=buffer;
            }
        }
        
        // Apply action if different than SLEEP (to avoid overwriting already scheduled slots)
        if(action != Action::SLEEP) {
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) { 
                result[slot] = ExplicitScheduleElement(action, e.getStreamInfo());
                if(buffer) result[slot].setBuffer(buffer);
            }
        }
    }
    print_dbg("[D] expandSchedule: allocated %d buffers\n",buffers.size());
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

bool ScheduleDownlinkPhase::checkTimeSetSchedule(long long slotStart) {
    // NOTE: The schedule can be explicited as soon as possible or toghether
    // with schedule activation, we decided to do it as soon as possible
    // because no info element can be sent during the slots in which we check
    // for activation.
    // Calculating the explicit schedule is possible and equivalent to what we do now
    if(explicitScheduleID != header.getScheduleID()) {
        auto myID = ctx.getNetworkId();
        explicitSchedule = expandSchedule(myID);
        explicitScheduleID = header.getScheduleID();
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG) {
            print_dbg("[SD] Calculated explicit schedule n.%2lu, tiles:%d, slots:%d\n",
                      explicitScheduleID, header.getScheduleTiles(), explicitSchedule.size());
            printExplicitSchedule(myID, true, explicitSchedule);
        }
    }
    // Schedule has been already activated, return true to resume the scheduler
    if(explicitScheduleID == dataPhase->getScheduleID())
        return true;
    unsigned int currentTile = ctx.getCurrentTile(slotStart);
    if (currentTile >= header.getActivationTile()) {
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG || ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            print_dbg("[SD] Activating schedule n.%2lu at tile n.%u\n", explicitScheduleID, currentTile);
        // Apply schedule to DataPhase
        dataPhase->applySchedule(explicitSchedule, explicitScheduleID,
                                 header.getScheduleTiles(),
                                 header.getActivationTile(), currentTile,
                                 currentTile > header.getActivationTile());
        // Apply schedule to StreamManager
        /* NOTE: we call applySchedule on the implicitSchedule to save time,
         * because implicit schedule is much smaller than explicit one */
        streamMgr->applySchedule(schedule);
        // Apply info elements to StreamManager
        streamMgr->applyInfoElements(infos);
        //NOTE: after we apply the schedule, we need to leave the time for connect() to return
        //in applications, and for them to call write(), otherwise the first transmission
        //fails due to no packet being available. If checkTimeSetSchedule returns immediately,
        //the code im MacContext will start executing the first slot of the data phase so early
        //that the stream in the first slot following the downlink phase does not have enough
        //time to do so.
        //Thus, we wait till a little before the end of the downlink slot.
        const int downlinkEndAdvance = std::max(MediumAccessController::receivingNodeWakeupAdvance,
                                                MediumAccessController::sendingNodeWakeupAdvance);
        ctx.sleepUntil(slotStart + ctx.getDownlinkSlotDuration() - downlinkEndAdvance);
        return true;
    }
    else{
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG || ENABLE_SCHEDULE_DIST_DYN_INFO_DBG)
            print_dbg("[SD] Too early to activate schedule! current:%2lu activation:%2lu\n",
                      currentTile, header.getActivationTile());
    }
    return false;
}

}

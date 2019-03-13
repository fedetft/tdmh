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
#include "../timesync/networktime.h"

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
        getCurrentSchedule(slotStart);
        printSchedule(0);
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG) {
            // Print schedule packet report
            print_dbg("[SD] Schedule Packet structure:\n");
            print_dbg("[SD] %d packet capacity\n", packetCapacity);
            print_dbg("[SD] %d schedule element\n", schedule.size());
        }
        // Reset variable for splitting schedule in packets
        position = 0;
        // Print explicit schedule of every node only on simulator
#ifndef _MIOSIX
        if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG)        
            printCompleteSchedule();
#endif
    }
    // ScheduleID = 0 means the first schedule is not ready
    if(header.getScheduleID() == 0) {
        // If InfoElements available, send a SchedulePkt with InfoElements only
        if(streamMgr->getNumInfo() != 0)
            sendInfoPkt(slotStart);
        return;
    }
    if(header.getRepetition() >= 3){
        // Stop after sending third schedule repetition
        // Then calculate the explicit schedule
        if(explicitScheduleID != header.getScheduleID()) {
            // Apply newest schedule by expanding it            
            auto myID = ctx.getNetworkId();
            auto newExplicitSchedule = expandSchedule(myID);
            explicitSchedule = std::move(newExplicitSchedule);
            explicitScheduleID = header.getScheduleID();
            if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG) {
                print_dbg("[SD] Calculated explicit schedule n.%2lu, tiles:%d, slots:%d\n",
                          explicitScheduleID, header.getScheduleTiles(), explicitSchedule.size());
                printExplicitSchedule(myID, true, explicitSchedule);
            }
        }
        checkTimeSetSchedule(slotStart);
        // If InfoElements available, send a SchedulePkt with InfoElements only
        if(streamMgr->getNumInfo() != 0)
            sendInfoPkt(slotStart);
        return;
    }
    if(header.getCurrentPacket() >= header.getTotalPacket()) {
        /* Reset schedule element index */
        position = 0;
        header.resetPacketCounter();
        header.incrementRepetition();
    }
    if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG)
        printHeader(header);
    sendSchedulePkt(slotStart);
    header.incrementPacketCounter();
}

void MasterScheduleDownlinkPhase::getCurrentSchedule(long long slotStart) {
    schedule = schedule_comp.getSchedule();
    auto currentTile = ctx.getCurrentTile(slotStart);
    auto activationTile = 0;
    unsigned numPackets = (schedule.size() / packetCapacity) + 1;
    auto tilesToDistributeSchedule = getTilesToDistributeSchedule(numPackets);
    // First schedule activation time
    if(schedule_comp.getScheduleID() == 1){
        auto superframeSize = ctx.getNetworkConfig().getControlSuperframeStructure().size();
        auto align = currentTile % superframeSize;
        if(align) currentTile += superframeSize - align;        
        activationTile = currentTile + tilesToDistributeSchedule;
    }
    // Subsequent schedules activation time
    else {
        // Use activationTile and scheduleTiles of the previous schedule (still saved in header)
        auto scheduleTiles = header.getScheduleTiles();
        auto currentScheduleTile = (currentTile - header.getActivationTile()) %
                                   scheduleTiles;
        auto remainingScheduleTiles = scheduleTiles - currentScheduleTile;
        activationTile = currentTile + remainingScheduleTiles;
        // Add multiples of scheduleTiles to allow schedule distribution
        if ((activationTile - currentTile) < tilesToDistributeSchedule) {
            auto moreTiles = (tilesToDistributeSchedule - (activationTile - currentTile));
            auto align = moreTiles % scheduleTiles;
            activationTile += moreTiles + (align ? scheduleTiles-align : 0);
        }
    }
    // Build a header for the new schedule
    ScheduleHeader newheader(
                             numPackets,                         // totalPacket
                             0,                                  // currentPacket
                             schedule_comp.getScheduleID(),      // scheduleID
                             activationTile,                     // activationTile
                             schedule_comp.getScheduleTiles());  // scheduleTiles
    header = newheader;
}

unsigned long MasterScheduleDownlinkPhase::getTilesToDistributeSchedule(unsigned int numPackets) {
    auto superframeSize = ctx.getNetworkConfig().getControlSuperframeStructure().size();
    auto downlinkInSuperframe = ctx.getNetworkConfig().getNumDownlinkSlotperSuperframe();
    // TODO: make number of repetitions configurable
    auto numRepetition = 3;
    auto downlinkNeeded = numPackets * numRepetition;
    auto result = downlinkNeeded / downlinkInSuperframe;
    if(downlinkNeeded % downlinkInSuperframe) result++;
    return result * superframeSize;
}

void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
    Packet pkt;
    // Add schedule distribution header
    header.serialize(pkt);
    // Add schedule elements to packet
    unsigned int sched = 0;
    for(sched = 0; (sched < packetCapacity) && (position < schedule.size()); sched++) {
        schedule[position].serialize(pkt);
        position++;
    }
    // Add info elements to packet
    unsigned char numInfo = packetCapacity - sched;
    auto infos = streamMgr->dequeueInfo(numInfo);
    for(auto& info : infos)
        info.serialize(pkt);
    // Send schedule downlink packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

void MasterScheduleDownlinkPhase::printHeader(ScheduleHeader& header) {
    print_dbg("[SD] sending schedule %u/%u/%lu/%d\n",
              header.getTotalPacket(),
              header.getCurrentPacket(),
              header.getScheduleID(),
              header.getRepetition());
}

void MasterScheduleDownlinkPhase::sendInfoPkt(long long slotStart) {
    Packet pkt;
    // Build Info packet header
    ScheduleHeader infoHeader(0,0,header.getScheduleID());
    // Add Info packet header
    infoHeader.serialize(pkt);
    // Add info elements to packet
    unsigned int availableInfo = streamMgr->getNumInfo();
    unsigned int numInfo = std::min(packetCapacity, availableInfo);
    auto infos = streamMgr->dequeueInfo(numInfo);
    for(auto& info : infos)
        info.serialize(pkt);
    // Send schedule downlink packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
}

}


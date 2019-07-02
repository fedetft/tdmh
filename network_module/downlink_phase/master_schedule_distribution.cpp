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

#include "master_schedule_distribution.h"
#include "../scheduler/schedule_computation.h"
#include "../tdmh.h"
#include "../util/packet.h"
#include "../util/debug_settings.h"
#include "../util/align.h"
#include "timesync/networktime.h"

using namespace miosix;

namespace mxnet {

MasterScheduleDownlinkPhase::MasterScheduleDownlinkPhase(MACContext& ctx,
                                                         ScheduleComputation& sch,
                                                         StreamCollection* scoll) :
    ScheduleDownlinkPhase(ctx), schedule_comp(sch), streamColl(scoll) {
    // Get number of downlink slots
    unsigned tileSize = ctx.getSlotsInTileCount();
    unsigned dataslotsDownlinktile = ctx.getDataSlotsInDownlinkTileCount();
    downlinkSlots = tileSize - dataslotsDownlinktile;
    packetCapacity = SchedulePacket::getPacketCapacity();
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
        if(streamColl->getNumInfo() != 0)
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
        // Try to apply schedule, if it has been applied, notify scheduler
        if(checkTimeSetSchedule(slotStart) == true)
            schedule_comp.scheduleApplied();
        // If InfoElements available, send a SchedulePkt with InfoElements only
        if(streamColl->getNumInfo() != 0)
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
    unsigned long id;
    unsigned int tiles;
    schedule_comp.getSchedule(schedule,id,tiles);
    unsigned int currentTile = ctx.getCurrentTile(slotStart);
    unsigned int activationTile = 0;
    unsigned int numPackets = (schedule.size()+packetCapacity-1) / packetCapacity;
    unsigned int tilesToDistributeSchedule = getTilesToDistributeSchedule(numPackets, currentTile);
    // Get scheduleTiles of the previous schedule (still saved in header)
    unsigned int lastScheduleTiles = header.getScheduleTiles();
    // If last schedule is empty, skip schedule alignment
    if(lastScheduleTiles == 0) {
        unsigned int superframeSize = ctx.getNetworkConfig().getControlSuperframeStructure().size();      
        // NOTE: tilesToDistributeSchedule is aligned to superframeSize
        activationTile = align(currentTile,superframeSize) + tilesToDistributeSchedule;
    }
    // Align new schedule to last schedule
    else {
        // Use activationTile of the previous schedule (still saved in header)
        unsigned int lastActivationTile = header.getActivationTile();
        if(currentTile < lastActivationTile)
            print_dbg("[SD] BUG! currentTile=%2lu < lastActivationTile=%2lu\n",
                      currentTile, lastActivationTile);
        unsigned int currentScheduleTile = (currentTile - lastActivationTile) %
                                           lastScheduleTiles;
        unsigned int remainingScheduleTiles = lastScheduleTiles - currentScheduleTile;
        activationTile = currentTile + remainingScheduleTiles;
        // Add multiples of lastScheduleTiles to allow schedule distribution
        if ((activationTile - currentTile) < tilesToDistributeSchedule) {
            unsigned int moreTiles = (tilesToDistributeSchedule - (activationTile - currentTile));
            unsigned int align = moreTiles % lastScheduleTiles;
            activationTile += moreTiles + (align ? lastScheduleTiles-align : 0);
        }
    }
    // Build a header for the new schedule
    ScheduleHeader newheader(
                             numPackets,         // totalPacket
                             0,                  // currentPacket
                             id,                 // scheduleID
                             activationTile,     // activationTile
                             tiles);             // scheduleTiles
    header = newheader;
}

unsigned int MasterScheduleDownlinkPhase::getTilesToDistributeSchedule(unsigned int numPackets,
                                                                       unsigned int currentTile) {
    unsigned int superframeSize = ctx.getNetworkConfig().getControlSuperframeStructure().size();
    unsigned int downlinkInSuperframe = ctx.getNetworkConfig().getNumDownlinkSlotperSuperframe();
    // TODO: make number of repetitions configurable
    unsigned int numRepetition = 3;
    unsigned int downlinkNeeded = numPackets * numRepetition;
    unsigned int superframeNeeded = downlinkNeeded / downlinkInSuperframe;
    if(downlinkNeeded % downlinkInSuperframe) superframeNeeded++;
    unsigned int tiles = superframeNeeded * superframeSize;
    unsigned int endTimesyncCount = ctx.getNumTimesyncs(currentTile + tiles);
    unsigned int beginTimesyncCount = ctx.getNumTimesyncs(currentTile);
    unsigned int numTimesync = endTimesyncCount - beginTimesyncCount;
    unsigned int result = tiles + (numTimesync + downlinkInSuperframe - 1) /
        downlinkInSuperframe * superframeSize;
    // FIXME: incomplete algorithm
    if ((ctx.getNumTimesyncs(currentTile + result) - beginTimesyncCount) > numTimesync) {
        result += superframeSize;
    }
    return result;
}

void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart) {
    SchedulePacket spkt;
    // Add ScheduleHeader to SchedulePacket
    spkt.setHeader(header);

    // Add schedule elements to SchedulePacket
    unsigned int sched = 0;
    for(sched = 0; (sched < packetCapacity) && (position < schedule.size()); sched++) {
        spkt.putElement(schedule[position]);
        position++;
    }
    // Add info elements to SchedulePacket
    unsigned char numInfo = packetCapacity - sched;
    auto infos = streamColl->dequeueInfo(numInfo);
    for(auto& info : infos)
        spkt.putInfoElement(info);

    Packet pkt;
    spkt.serialize(pkt, panId);
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
    SchedulePacket spkt;
    // Build Info packet header
    ScheduleHeader infoHeader(0,0,header.getScheduleID());
    spkt.setHeader(infoHeader);

    // Add info elements to SchedulePacketacket
    unsigned int availableInfo = streamColl->getNumInfo();
    unsigned int numInfo = std::min(packetCapacity, availableInfo);
    std::vector<InfoElement> infos = streamColl->dequeueInfo(numInfo);
    for(auto& info : infos)
        spkt.putInfoElement(info);

    Packet pkt;
    spkt.serialize(pkt, panId);
    // Send schedule downlink packet
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
    // NOTE: Apply vector of Info elements to local StreamManager
    streamMgr->applyInfoElements(infos);
}

}


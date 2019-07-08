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
    // If a new schedule is available
    if(schedule_comp.getScheduleID() != header.getScheduleID()) {
        getCurrentSchedule(slotStart);
        distributing = true;
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
        // If we updated the schedule, wait for the next Downlink before
        // sending the first packet
        return;
    }
    if(distributing == false) {
        // If InfoElements available, send a SchedulePkt with InfoElements only
        if(streamColl->getNumInfo() != 0)
            sendInfoPkt(slotStart);
        return;
    } else {
        // If we are still sending the schedule
        if(header.getRepetition() < 3) {
            if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG)
                printHeader(header);
            // NOTE: schedulePkt includes also info elements
            sendSchedulePkt(slotStart);
            header.incrementPacketCounter();
            if(header.getCurrentPacket() >= header.getTotalPacket()) {
                /* Reset schedule element index */
                position = 0;
                header.resetPacketCounter();
                header.incrementRepetition();
            }
        }
        // If we already sent the schedule three times
        else {
            // Try to apply schedule, if applied, notify the scheduler
            if(checkTimeSetSchedule(slotStart) == true) {
                schedule_comp.scheduleApplied();
                distributing = false;
            }
        }
    }
}

void MasterScheduleDownlinkPhase::getCurrentSchedule(long long slotStart) {
    unsigned long id;
    unsigned int tiles;
    schedule_comp.getSchedule(schedule,id,tiles);
    unsigned int currentTile = ctx.getCurrentTile(slotStart);
    //NOTE: An empty schedule still requires 1 packet to send the scheduleHeader
    unsigned int numPackets = std::max<unsigned int>(1,(schedule.size()+packetCapacity-1) / packetCapacity);
    
    // Get the earliest tile when we can activate the schedule, not considering
    // that it must be aligned to the end of the previous schedule, if there is one
    unsigned int activationTile = getActivationTile(currentTile, numPackets);
    
    // Get scheduleTiles of the previous schedule (still saved in header)
    unsigned int lastScheduleTiles = header.getScheduleTiles();
    if(lastScheduleTiles > 0)
    {
        // Use activationTile of the previous schedule (still saved in header)
        unsigned int lastActivationTile = header.getActivationTile();
        if(currentTile < lastActivationTile)
            print_dbg("[SD] BUG! currentTile=%2lu < lastActivationTile=%2lu\n",
                      currentTile, lastActivationTile);
        
        // The first beginning of a schedule that is at or after activationTile
        unsigned int alignedActivationTile = lastActivationTile;
        alignedActivationTile += (activationTile + lastScheduleTiles - 1 - lastActivationTile)
                                 / lastScheduleTiles * lastScheduleTiles;
        
        // But wait, there's more corner cases! The aligned activation tile
        // must not be a timesync, if it is, we have to postpone activation
        // by a full (old) schedule
        unsigned int isActivationTileATimesync = ctx.getNumTimesyncs(alignedActivationTile + 1)
                                               - ctx.getNumTimesyncs(alignedActivationTile);
        
        if(isActivationTileATimesync) alignedActivationTile += lastScheduleTiles;
        
        unsigned int bugTwoConsecutiveTimesyncs = ctx.getNumTimesyncs(alignedActivationTile + 1)
                                                - ctx.getNumTimesyncs(alignedActivationTile);
        if(bugTwoConsecutiveTimesyncs)
            print_dbg("[SD] BUG! two consecutive timesyncs (aat=%u, lst=%u lat=%u)\n",
                      alignedActivationTile, lastScheduleTiles, lastActivationTile);

        activationTile = alignedActivationTile;
    }
    
    // Build a header for the new schedule                    
    header = ScheduleHeader(
        numPackets,         // totalPacket
        0,                  // currentPacket
        id,                 // scheduleID
        activationTile,     // activationTile
        tiles);             // scheduleTiles
}

unsigned int MasterScheduleDownlinkPhase::getActivationTile(unsigned int currentTile,
                                                            unsigned int numPackets)
{
    // This function assumes that in the current tile no packet will be sent,
    // then 3*numPackets need to be sent, one per free downlink (i.e a downlink
    // not occupied by a timesync), and the activation tile needs to be the
    // first free downlink after the last packet has been sent.
    // NOTE: the activtion tile also needs to be aligned to a control superframe
    // as the schedule has "holes" for the downlink and uplink, which are of
    // different number of slots.
    unsigned int numDownlinks = 3 * numPackets;
    
    // The first tile that we consider is currentTile + 1 because in
    // currentTile no packet is sent
    const unsigned int firstTile = currentTile + 1;
    
    auto cs = ctx.getNetworkConfig().getControlSuperframeStructure();
    const unsigned int csSize = cs.size();
    const unsigned int csDownlinks = cs.countDownlinkSlots();
    
    // First, since a control superframe can have multiple downlinks, we need
    // to align to the beginning of a control superframe
    unsigned int activationTile = firstTile;
    unsigned int phase = firstTile % csSize;
    if(phase != 0)
    {
        while(phase < csSize)
        {
            if(cs.isControlDownlink(phase) && numDownlinks>0) numDownlinks--;
            phase++;
            activationTile++;
        }
    } // else we're already aligned
    
    // Now we compute a tentative activationTile without considering
    // that some downlinks may be unavailable due to clocksyncs, and
    // at the end we consider timesyncs. As adding more control superframes
    // to account for clocksyncs may encompass even more clocksyncs, we need
    // to iterate until no more timesyncs occur in the newly added superframes
    unsigned int begin = firstTile;
    for(int i = 0;;i++)
    {
        assert(i < 10);
        
        const unsigned int numControlSuperframes = numDownlinks / csDownlinks;
        
        activationTile += numControlSuperframes * csSize;
        numDownlinks   -= numControlSuperframes * csDownlinks;
        
        // If numDownlinks is not divisible by csDownlinks, we add a full
        // control superframe (remember that activation tile must be aligned?)
        // but we also note the free downlinks that may remain
        unsigned int remaining = 0;
        if(numDownlinks > 0)
        {
            activationTile += csSize;
            assert(csDownlinks >= numDownlinks);
            remaining = csDownlinks - numDownlinks;
        }

        unsigned int numTimesyncs = ctx.getNumTimesyncs(activationTile)
                                  - ctx.getNumTimesyncs(begin);
                                  
        unsigned int isActivationTileATimesync = ctx.getNumTimesyncs(activationTile + 1)
                                               - ctx.getNumTimesyncs(activationTile);
        
        // Three possible cases to handle
        if(numTimesyncs > remaining)
        {
            // We need more downlinks to send packets, set numDownlinks and redo
            // because adding more control superframes to account for clocksyncs
            // may encompass even more clocksyncs
            numDownlinks = numTimesyncs - remaining;
            begin = activationTile;
        } else if(isActivationTileATimesync == 1) {
            // Here we have our last corner case. The activation tile must not
            // be a timesync. Even if there are remaining downlinks free, we
            // must advance to the next control superframe or we would activate
            // the schedule late. Note that if this control superframe starts
            // with a timesync, the next one will not, so no need to iterate
            activationTile += csSize;
            break;
        } else {
            // Nothing to do 
            break;
        }
    }
    
    return activationTile;
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


/**************************************************************************
 *   Copyright (C) 2018-2019 by Federico Amedeo Izzo and Federico Terraneo *
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
#include "../stream/stream_parameters.h"
#include <vector>
#include <set>

using namespace miosix;

namespace mxnet {

MasterScheduleDownlinkPhase::MasterScheduleDownlinkPhase(MACContext& ctx,
                                                         ScheduleComputation& sch) :
    ScheduleDownlinkPhase(ctx), schedule_comp(sch),
    streamColl(sch.getStreamCollection()) {}

void MasterScheduleDownlinkPhase::execute(long long slotStart)
{
    switch(status)
    {
        case ScheduleDownlinkStatus::APPLIED_SCHEDULE:
        {
            if(schedule_comp.needToSendSchedule())
            {
                getScheduleAndComputeActivation(slotStart);
                status = ScheduleDownlinkStatus::SENDING_SCHEDULE;
                //No packet sent in this downlink slot
            } else {
                if(streamColl->getNumInfo() != 0 || ctx.getKeyManager()->challengesPresent()) {
                    sendInfoPkt(slotStart);
                }
            }
            break;
        }
        case ScheduleDownlinkStatus::SENDING_SCHEDULE:
        {
            if(currentSendingRound >= sendingRounds) {
                setNewSchedule(slotStart);
                status = ScheduleDownlinkStatus::REKEYING;
            } else {
                sendSchedulePkt(slotStart);
                header.incrementPacketCounter();
                if(header.getCurrentPacket() >= header.getTotalPacket())
                {
                    position = 0;
                    header.resetPacketCounter();
                    header.incrementRepetition();
                }
            }
            currentSendingRound++;
            break;
        }
        case ScheduleDownlinkStatus::REKEYING:
        {
            unsigned int currentTile = ctx.getCurrentTile(slotStart);
            if(currentTile >= header.getActivationTile())
            {
                applyNewSchedule(slotStart);
                schedule_comp.scheduleSentAndApplied();
                status = ScheduleDownlinkStatus::APPLIED_SCHEDULE;
                //No packet sent in this downlink slot
            } else {
                // Keep rekeying streams if needed
#ifdef CRYPTO
                streamMgr->continueRekeying();
#endif
                // We don't send any info elements here, as the dynamic nodes are busy
                // rekeying too
            }
            break;
        }
        default:
            assert(false);
    }
}

void MasterScheduleDownlinkPhase::getScheduleAndComputeActivation(long long slotStart)
{
    unsigned long id;
    unsigned int tiles;
    schedule_comp.getSchedule(schedule,id,tiles);
    unsigned int currentTile = ctx.getCurrentTile(slotStart);
    //NOTE: An empty schedule still requires 1 packet to send the scheduleHeader
    unsigned int numPackets = std::max<unsigned int>(1,(schedule.size()+packetCapacity-1) / packetCapacity);

    //Initialize sendingRounds
    sendingRounds = numPackets * scheduleRepetitions;
    currentSendingRound = 0;

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
        {
            if(ENABLE_SCHEDULE_DIST_DBG)
                print_dbg("[SD] BUG! currentTile=%2lu < lastActivationTile=%2lu\n",
                          currentTile, lastActivationTile);
        }
        
        // The first beginning of a schedule that is at or after activationTile
        unsigned int alignedActivationTile = lastActivationTile;
        alignedActivationTile += align(activationTile - lastActivationTile, lastScheduleTiles);
        
        // But wait, there's more corner cases! The aligned activation tile
        // must not be a timesync, if it is, we have to postpone activation
        // by a full (old) schedule
        unsigned int isActivationTileATimesync = ctx.getNumTimesyncs(alignedActivationTile + 1)
                                               - ctx.getNumTimesyncs(alignedActivationTile);
        
        if(isActivationTileATimesync) alignedActivationTile += lastScheduleTiles;
        
        unsigned int bugTwoConsecutiveTimesyncs = ctx.getNumTimesyncs(alignedActivationTile + 1)
                                                - ctx.getNumTimesyncs(alignedActivationTile);
        if(bugTwoConsecutiveTimesyncs)
        {
            if(ENABLE_SCHEDULE_DIST_DBG)
                print_dbg("[SD] BUG! two consecutive timesyncs (aat=%u, lst=%u lat=%u)\n",
                          alignedActivationTile, lastScheduleTiles, lastActivationTile);
        }

        activationTile = alignedActivationTile;
    }
    
    // Build a header for the new schedule                    
    header = ScheduleHeader(
        numPackets,         // totalPacket
        0,                  // currentPacket
        id,                 // scheduleID
        activationTile,     // activationTile
        tiles);             // scheduleTiles
    
    if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG)
    {
        // Print schedule packet report
        print_dbg("[SD] Schedule Packet structure:\n");
        print_dbg("[SD] %d packet capacity\n", packetCapacity);
        print_dbg("[SD] %d schedule element\n", schedule.size());
    }

    position = 0;
    
    if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG)        
        printCompleteSchedule();
}

unsigned int MasterScheduleDownlinkPhase::getActivationTile(unsigned int currentTile,
                                                            unsigned int numPackets)
{
    // This function assumes that in the current tile no packet will be sent,
    // then scheduleRepetitions*numPackets need to be sent,
    // one per free downlink (i.e a downlink not occupied by a timesync),
    // and the activation tile needs to be the first free downlink after the
    // last packet has been sent.
    // NOTE: the activtion tile also needs to be aligned to a control superframe
    // as the schedule has "holes" for the downlink and uplink, which are of
    // different number of slots.
    unsigned int numDownlinks = scheduleRepetitions * numPackets;
    numDownlinks += getNumDownlinksForRekeying();
    
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

unsigned int MasterScheduleDownlinkPhase::getNumDownlinksForRekeying() {
#ifdef CRYPTO
    // Compute the worst case: max number of streams that any node must rekey
    std::vector<unsigned int> streamsPerNode;
    streamsPerNode.resize(ctx.getNetworkConfig().getMaxNodes());
    std::fill(streamsPerNode.begin(), streamsPerNode.end(), 0);

    std::set<StreamId> streams;
    for (auto e : schedule) {
        StreamId id = e.getStreamInfo().getStreamId();
        /* If we have already counted this stream, do nothing. Otherwise count it
         * for both endpoints */
        auto it = streams.find(id);
        if(it == streams.end()) {
            streamsPerNode[e.getSrc()]++;
            streamsPerNode[e.getDst()]++;
            streams.insert(id);
        }
    }

    /* Take the maximum of streamPerNode vector */
    unsigned int maxStreams = 1;
    for (auto n : streamsPerNode) {
        if (n > maxStreams) maxStreams = n;
    }

    // Leave enough downlink slots for all streams to be rekeyed. Always
    // leave at least one slot to change state.
    unsigned int hashesPerSlot = streamMgr->getMaxHashesPerSlot();
    return 1 + align(maxStreams, hashesPerSlot) / hashesPerSlot;
#else
    return 1;
#endif
}

void MasterScheduleDownlinkPhase::sendSchedulePkt(long long slotStart)
{
    if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG) printHeader(header);
    
    SchedulePacket spkt(panId);
    // Add ScheduleHeader to SchedulePacket
    spkt.setHeader(header);

    // Add schedule elements to SchedulePacket
    unsigned int sched = 0;
    for(sched = 0; (sched < packetCapacity) && (position < schedule.size()); sched++)
    {
        spkt.putElement(schedule[position]);
        position++;
    }
    // Add info elements to SchedulePacket
    unsigned char numInfo = packetCapacity - sched;
    auto infos = streamColl->dequeueInfo(numInfo);
    for(auto& info : infos) spkt.putInfoElement(info);

    Packet pkt;
#ifdef CRYPTO
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        pkt.reserveTag();
    }
#endif
    spkt.serialize(pkt);
    // Send schedule downlink packet
    sendPkt(slotStart,pkt);
    // NOTE: Apply vector of Info elements to local StreamManager
    streamMgr->applyInfoElements(infos);
}

void MasterScheduleDownlinkPhase::sendInfoPkt(long long slotStart)
{
    SchedulePacket spkt(panId);
    // Build Info packet header
    ScheduleHeader infoHeader(0,0,header.getScheduleID());
    spkt.setHeader(infoHeader);

    unsigned int capacity = packetCapacity;
#ifdef CRYPTO
    // Solve challenges if needed and put them in the packet
    if(ctx.getNetworkConfig().getDoMasterChallengeAuthentication()) {
        auto responses = ctx.getKeyManager()->solveChallengesAndGetResponses();
        capacity -= responses.size();
        for(auto& r : responses) spkt.putInfoElement(r);
    }
#endif

    // Add info elements to SchedulePacketacket
    auto infos = streamColl->dequeueInfo(capacity);
    for(auto& info : infos) spkt.putInfoElement(info);

    Packet pkt;
#ifdef CRYPTO
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        pkt.reserveTag();
    }
#endif
    spkt.serialize(pkt);
    // Send schedule downlink packet
    sendPkt(slotStart,pkt);
    // NOTE: Apply vector of Info elements to local StreamManager
    streamMgr->applyInfoElements(infos);
}

void MasterScheduleDownlinkPhase::sendPkt(long long slotStart, Packet& pkt)
{
#ifdef CRYPTO
    if(ctx.getNetworkConfig().getAuthenticateControlMessages()) {
        AesGcm& gcm = ctx.getKeyManager()->getScheduleDistributionGCM();
        unsigned int tileNumber = ctx.getCurrentTile(slotStart);
        unsigned int seqNo = 1;
        unsigned int masterIndex = ctx.getKeyManager()->getMasterIndex();
        if(ENABLE_CRYPTO_DOWNLINK_DBG)
            print_dbg("[SD] Authenticating downlink: tile=%u, seqNo=%llu, mI=%u\n",
                      tileNumber, seqNo, masterIndex);
        gcm.setIV(tileNumber, seqNo, masterIndex);
        if(ctx.getNetworkConfig().getEncryptControlMessages()) {
            pkt.encryptAndPutTag(gcm);
        } else {
            pkt.putTag(gcm);
        }
    }
#endif

#if FLOOD_TYPE == 0
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    pkt.send(ctx, slotStart);
    ctx.transceiverIdle();
#elif FLOOD_TYPE == 1
    ctx.configureTransceiver(ctx.getTransceiverConfig());
    int maxHop = ctx.getNetworkConfig().getMaxHops();
    bool send = true; //Flood initiator, first time, send
    for(int slot = 0; slot < maxHop; slot++)
    {
        if(send)
        {
            pkt.send(ctx, slotStart);
            slotStart += rebroadcastInterval;
            send = false; //Sent: next time, receive
        } else {
            auto rcvResult = pkt.recv(ctx, slotStart);
            if(rcvResult.error == RecvResult::ErrorCode::OK && pkt.checkPanHeader(panId) == true)
            {
                slotStart = rcvResult.timestamp + rebroadcastInterval;
                send = true; //Received successfully: next time, send
            } else slotStart += rebroadcastInterval;
        }
        
    }
    ctx.transceiverIdle();
#else
#error
#endif
}

void MasterScheduleDownlinkPhase::printHeader(ScheduleHeader& header)
{
    print_dbg("[SD] sending schedule %u/%u/%lu/%d\n",
              header.getTotalPacket(),
              header.getCurrentPacket(),
              header.getScheduleID(),
              header.getRepetition());
}

}


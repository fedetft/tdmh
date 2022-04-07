/***************************************************************************
 *   Copyright (C) 2018-2022 by Federico Amedeo Izzo, Valeria Mazzola,     *
 *   Luca Conterio                                                         *
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

#include "schedule_expansion.h"

namespace mxnet {

ScheduleExpander::ScheduleExpander(MACContext& c) : ctx(c), netConfig(c.getNetworkConfig()), streamMgr(c.getStreamManager()) {
    // rounded to lower bound
    expansionsPerSlot = ctx.getDownlinkSlotDuration() / singleExpansionTime;

    slotsInTile = ctx.getSlotsInTileCount();
    slotsInSuperframe = netConfig.getControlSuperframeStructure().size() * ctx.getSlotsInTileCount();
    superframeSize = netConfig.getControlSuperframeStructure().size();
}

void ScheduleExpander::startExpansion(const std::vector<ScheduleElement>& schedule, 
                                        const ScheduleHeader& header, unsigned char node) {
    std::unique_lock<std::mutex> lck(expansionMutex);

    if (expansionInProgress) {
        //if(ENABLE_SCHEDULE_DIST_DBG) {
        //    print_dbg("[SD] N=%d BUG: call to startExpansion while schedule expansion is already in progress\n", nodeID);
        //}
        return;
    }

    nodeID = node;

    // Reset all the needed varibales and data structures
    expansionInProgress = true;
    expansionIndex = 0;
    explicitScheduleComplete  = false;
    activationTile = header.getActivationTile();
    activationTime = activationTile * netConfig.getTileDuration();

    // Resize new explicit schedule and fill with default value (sleep)
    explicitSchedule = std::vector<ExplicitScheduleElement>();
    scheduleTiles    = header.getScheduleTiles();
    scheduleDuration = scheduleTiles * netConfig.getTileDuration();
    scheduleSlots    = scheduleTiles * slotsInTile;
    explicitSchedule.resize(scheduleSlots, ExplicitScheduleElement());

    forwardedStreamCtr = std::map<StreamId, std::pair<unsigned char, unsigned char>>();
    buffers            = std::map<unsigned int, std::shared_ptr<Packet>>();
    uniqueStreams      = std::set<unsigned int>();

    // Count unique streams in schedule and
    // preallocate space in streams wakeup lists
    std::pair<unsigned int, unsigned int> countPair = countStreams(schedule);
    // Total number of unique streams in the schedule
    unsigned int numScheduledStreams          = countPair.first;
    // Number of streams whose wakeup advance is big enough
    // to require them to be woken up during the previous tile
    unsigned int numStreamsWithNegativeOffset = countPair.second;

    // Count number of downlink slots in complete schedule
    numDownlinksInSchedule = countDownlinks();
    addedDownlinksNum = 0;

    // Reserve space for streams wakeup lists
    currList = std::vector<StreamWakeupInfo>();
    currList.reserve(numScheduledStreams - numStreamsWithNegativeOffset + numDownlinksInSchedule);
    nextList = std::vector<StreamWakeupInfo>();
    nextList.reserve(numStreamsWithNegativeOffset);

    downlinksIndex = 0;
    nextDownlinkTime = findNextDownlinkTime();

    if(ENABLE_SCHEDULE_DIST_DBG) {
        print_dbg("[SD] N=%d, Expansion started, NT=%llu\n", nodeID, NetworkTime::now().get());
    }
}

void ScheduleExpander::continueExpansion(const std::vector<ScheduleElement>& schedule, bool setWakeupLists) {
    std::unique_lock<std::mutex> lck(expansionMutex);

    if (!expansionInProgress) {
        return;
    }
    

    if (!explicitScheduleComplete) { // schedule expansion not finished yet
        // if(ENABLE_SCHEDULE_DIST_DBG) {
        //     print_dbg("N=%d, Expansion continues, NT=%llu\n", nodeID, NetworkTime::now().get());
        // }

        expandSchedule(schedule);
        
        if (explicitScheduleComplete) { // schedule expansion can now be finished

            if(ENABLE_SCHEDULE_DIST_DBG) {
                print_dbg("[SD] N=%d, expandSchedule: allocated %d buffers\n", nodeID, buffers.size());
            }
            
            if(explicitSchedule.size() != scheduleSlots) {
                print_dbg("[SD] N=%d, BUG: Schedule expansion inconsistency\n", nodeID);
            }

            if (setWakeupLists) {
                streamMgr->setStreamsWakeupLists(currList, nextList);
            }

            if (ENABLE_SCHEDULE_DIST_DBG) {
                print_dbg("[SD] N=%d, Expansion complete, NT=%llu\n", nodeID, NetworkTime::now().get());
            }

            // all operations done
            expansionInProgress = false;
        }
    }
}

bool ScheduleExpander::needToContinueExpansion() {
    return !explicitScheduleComplete;
}

const std::vector<ExplicitScheduleElement>& ScheduleExpander::expandScheduleOneShot(const std::vector<ScheduleElement>& schedule, 
                                                                                        const ScheduleHeader& header, unsigned char node,
                                                                                        bool setStreamWakeupList) {
    startExpansion(schedule, header, node);
    while(needToContinueExpansion()) {
        continueExpansion(schedule, setStreamWakeupList);
    }

    return explicitSchedule;
}

unsigned int ScheduleExpander::getExpansionsPerSlot() const {
    return expansionsPerSlot;
}

const std::map<StreamId, std::pair<unsigned char, unsigned char>>& ScheduleExpander::getForwardedStreams() const {
    return forwardedStreamCtr;
}

const std::vector<ExplicitScheduleElement>& ScheduleExpander::getExplicitSchedule() {
    return explicitSchedule;
}

void ScheduleExpander::expandSchedule(const std::vector<ScheduleElement>& schedule)
{
    if(ENABLE_SCHEDULE_DIST_DBG) {
        print_dbg("[SD] N=%d, Expanding schedule at NT=%llu\n", nodeID, NetworkTime::now().get());
    }

    // Get index reached at last call to expandSchedule(), during last downlink slot
    unsigned int lastIndex = expansionIndex;

    // Scan implicit schedule for element that imply the node action
    while (expansionIndex - lastIndex < expansionsPerSlot && expansionIndex < schedule.size())
    {   
        auto e = schedule[expansionIndex];

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
        else if(e.getSrc() != nodeID && e.getTx() == nodeID)
        {
            action = Action::SENDBUFFER;
            auto it=buffers.find(e.getStreamId().getKey());
            if(it!=buffers.end())
            {
                buffer=it->second;
            } else {
                //Should never happen, how can we transmit from a buffer we haven't received from?
                if(ENABLE_SCHEDULE_DIST_DBG) {
                    print_dbg("[SD] Error: expandSchedule missing buffer\n");
                }
            }

            {
                // Look for this stream in map to set and increment counter
                StreamId id = e.getStreamId();
                auto it = forwardedStreamCtr.find(id);
                if(it != forwardedStreamCtr.end()) {
                    it->second.second++;
                } else {
                    forwardedStreamCtr[id] = std::make_pair(0,1);
                }
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
        if(action != Action::SLEEP)
        {
            // For those streams of the current node that have to send something,
            // their info will be added to a table, which will be later used by the StreamWakeupScheduler
            bool streamNotYetInserted = false;
            unsigned int streamMinOffset = scheduleSlots;
            if (action == Action::SENDSTREAM) {
                // if stream found for first time
                // (only consider the first of the redundant apparisons of stream)
                if (uniqueStreams.find(e.getKey()) == uniqueStreams.end()) {
                    streamNotYetInserted = true;
                    // not guaranteed that the first stream occurrence has
                    // the minimum offset among all the other occurrences, 
                    // so find the minimum one
                    for(unsigned int i = 0; i < schedule.size(); i++) {
                        // if action == Action::SENDSTREAM
                        if(schedule[i].getSrc() == nodeID && schedule[i].getTx() == nodeID) {
                            auto off = schedule[i].getOffset();
                            if (off < streamMinOffset) {
                                streamMinOffset = off;
                            }
                        }
                    }
                }
            }

            int periodicRepetitions = 0; // iterations counter
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) {
                explicitSchedule[slot] = ExplicitScheduleElement(action, e.getStreamInfo());
                if(buffer) 
                    explicitSchedule[slot].setBuffer(buffer);

                if (action == Action::SENDSTREAM) {
                    // only use first appearance of stream in schedule as an offset,
                    // add the periodic occurrencies of the same stream too
                    if (streamNotYetInserted) {
                        auto wakeupAdvance = streamMgr->getWakeupAdvance(e.getStreamId());
                        if (wakeupAdvance > 0) { // otherwise no need to wakeup it when requested
                            uniqueStreams.insert(e.getKey());
                            // Create and add StreamWakeupInfo to stream wakeup list
                            // Add all the appearances of the stream
                            auto offset = streamMinOffset + periodicRepetitions * periodSlots;
                            addStream(e, offset, wakeupAdvance, activationTile);
                        }
                    }
                }

                periodicRepetitions++;
            }
        }

        expansionIndex++;
    }

    if (addedDownlinksNum < numDownlinksInSchedule) {
        while(addedDownlinksNum < numDownlinksInSchedule) {
            addDownlink(nextDownlinkTime, currList);
        }
    }
    
    if (expansionIndex == schedule.size()) {
        // Computation of the explicit schedule done
        explicitScheduleComplete = true;
    }
}

void ScheduleExpander::addStream(const ScheduleElement& stream, 
                                                            unsigned int offset,
                                                            unsigned long long wakeupAdvance, 
                                                            unsigned int activationTile) {

    bool negativeSlot = false;

    int wakeupSlot = getWakeupSlot(offset, wakeupAdvance);
    
    if (wakeupSlot < 0) {
        negativeSlot = true;
        // in "next list" only stream with wakeup time
        // lower than schedule activation time basically
        // (since offsets go from 0 to scheduleSlots-1)
        wakeupSlot = scheduleSlots + wakeupSlot;
    }

    unsigned int tileIndexInSuperframe = wakeupSlot / slotsInTile;
    unsigned int slackTime =  tileIndexInSuperframe * ctx.getTileSlackTime();
    unsigned int wakeupTimeOffset = (wakeupSlot * ctx.getDataSlotDuration()) + slackTime;

    StreamWakeupInfo swi{WakeupInfoType::WAKEUP_STREAM, stream.getStreamId(), wakeupTimeOffset};

    if (negativeSlot) {
        // In this case, the wakeup time offset (wakeup slot) is relative
        // to the previous schedule w.r.t. to the new activation one
        auto baseTime = activationTime - scheduleDuration;
        swi.wakeupTime += NetworkTime::fromNetworkTime(baseTime).toLocalTime();
    }
    else {
        // The wakeup time offset (wakeup slot) is relative to the activation tile
        // (so the first time this stream will have to be woken up is the schedule
        // activation time + the stream's offset)
        swi.wakeupTime += NetworkTime::fromNetworkTime(activationTime).toLocalTime();
    }

    if (nextDownlinkTime <= swi.wakeupTime) {
        // Avoid inserting more downlinks than needed
        if (addedDownlinksNum < numDownlinksInSchedule) {
            addDownlink(nextDownlinkTime, currList);
        }
    }

    // If wakeup slot was negative, add element to the "next" list, otherwise to the "curr" list
    // to maintan separated the wakeup times relative to the next or to the current superframe
    if (negativeSlot) {
        // ordered insert
        nextList.insert(std::upper_bound(nextList.begin(), nextList.end(), swi), swi);
        negativeSlot = false;
    }
    else {
        // ordered insert
        currList.insert(std::upper_bound(currList.begin(), currList.end(), swi), swi);
    }
}

unsigned long long ScheduleExpander::findNextDownlinkTime() {

    int tileIndex = downlinksIndex % superframeSize; // used to iterate over control superframe tiles
    bool found = false;
    ControlSuperframeStructure superframeStructure = netConfig.getControlSuperframeStructure();

    while (!found) {

        if (superframeStructure.isControlDownlink(tileIndex)) {
            // time of downlink end time
            nextDownlinkTime = (downlinksIndex * netConfig.getTileDuration()) + ctx.getDownlinkSlotDuration();
            nextDownlinkTime += NetworkTime::fromNetworkTime(activationTime).toLocalTime();
            found = true;
        }

        downlinksIndex++;
    
        tileIndex++;
        if (tileIndex == superframeSize) {
            tileIndex = 0;
        }
    }
    
    return nextDownlinkTime;
}

void ScheduleExpander::addDownlink(unsigned long long downlinkTime, std::vector<StreamWakeupInfo>& list) {
    StreamWakeupInfo swi{WakeupInfoType::WAKEUP_DOWNLINK, StreamId(), downlinkTime};
    // ordered insert
    currList.insert(std::upper_bound(currList.begin(), currList.end(), swi), swi);
    nextDownlinkTime = findNextDownlinkTime();
    addedDownlinksNum++;
}

unsigned int ScheduleExpander::getWakeupSlot(unsigned int offset, unsigned long long wakeupAdvance) {
    return offset - (wakeupAdvance / ctx.getDataSlotDuration());
}

std::pair<unsigned int, unsigned int> ScheduleExpander::countStreams(const std::vector<ScheduleElement>& schedule) {
    // Implicit schedule contains repeated streams, according to their
    // redundancy value, keep track of existing streams
    std::set<StreamId> countedStreams;

    unsigned int numNegativeOffset = 0;
    unsigned int numTotal = 0;

    for (auto e : schedule) {

        // count only this node's streams that have to send
        if (e.getSrc() == nodeID && e.getTx() == nodeID) { // action == Action::SENDSTREAM
            
            auto periodSlots = toInt(e.getPeriod()) * slotsInTile;
            auto it = countedStreams.find(e.getStreamId());
            
            // first time we encounter this stream,
            // avoid counting also slots assigned to redundant transmissions
            if (it == countedStreams.end()) {
                countedStreams.insert(e.getStreamId());
                
                for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots) {
                    numTotal++;
                    auto wakeupAdvance = streamMgr->getWakeupAdvance(e.getStreamId());

                    int wakeupSlot = getWakeupSlot(slot, wakeupAdvance);
                    if (wakeupSlot < 0) {
                        numNegativeOffset++;
                    }
                }
            }
        }
    }

    return std::make_pair(numTotal, numNegativeOffset);
}

unsigned int ScheduleExpander::countDownlinks() {
    // number of control superframes in complete schedule
    unsigned int numSuperframesInSchedule = scheduleTiles / superframeSize;
    // number of downlinks per control superframe * number of control superframes in complete schedule
    unsigned int numDownlinksPerSchedule = netConfig.getControlSuperframeStructure().countDownlinkSlots() * numSuperframesInSchedule;

    return numDownlinksPerSchedule;

}

}; // namespace mxnet
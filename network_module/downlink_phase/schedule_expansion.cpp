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

void ScheduleExpander::startExpansion(const std::vector<ScheduleElement>& schedule, const ScheduleHeader& header) {
    std::unique_lock<std::mutex> lck(expansionMutex);

    if (inProgress) {
        if(ENABLE_SCHEDULE_DIST_DBG) {
            print_dbg("[SD] N=%d BUG: call to startExpansion while schedule expansion is already in progress\n", ctx.getNetworkId());
        }
        return;
    }

    // Reset all the needed varibales and data structures
    inProgress = true;
    expansionIndex = 0;
    explicitScheduleComplete  = false;
    activationTile = header.getActivationTile();

    // Resize new explicit schedule and fill with default value (sleep)
    explicitSchedule = std::vector<ExplicitScheduleElement>();
    scheduleSlots = header.getScheduleTiles() * slotsInTile;
    explicitSchedule.resize(scheduleSlots, ExplicitScheduleElement());

    forwardedStreamCtr = std::map<StreamId, std::pair<unsigned char, unsigned char>>();
    buffers            = std::map<unsigned int, std::shared_ptr<Packet>>();
    uniqueStreams      = std::set<StreamId>();

    // Count unique streams in schedule and
    // preallocate space in streams wakeup lists
    std::pair<unsigned int, unsigned int> countPair = countStreams(schedule);
    // Total number of unique streams in the schedule
    unsigned int numScheduledStreams          = countPair.first;
    // Number of streams whose wakeup advance is big enough
    // to require them to be woken up during the previous tile
    unsigned int numStreamsWithNegativeOffset = countPair.second;

    // Reserve space for streams wakeup lists
    currList = std::vector<StreamWakeupInfo>();
    currList.reserve(numScheduledStreams - numStreamsWithNegativeOffset
                            + netConfig.getControlSuperframeStructure().countDownlinkSlots());
    nextList = std::vector<StreamWakeupInfo>();
    nextList.reserve(numStreamsWithNegativeOffset);

    if(ENABLE_SCHEDULE_DIST_DBG) {
        print_dbg("[SD] N=%d, Expansion started at NT=%llu\n", ctx.getNetworkId(), NetworkTime::now().get());
    }
}

void ScheduleExpander::continueExpansion(const std::vector<ScheduleElement>& schedule) {
    std::unique_lock<std::mutex> lck(expansionMutex);

    if (!inProgress) {
        if(ENABLE_SCHEDULE_DIST_DBG) {
            print_dbg("[SD] N=%d BUG: call to continueExpansion without starting schedule expansion first\n", ctx.getNetworkId());
        }
        return;
    }
    

    if (!explicitScheduleComplete) { // schedule expansion not finished yet
        // if(ENABLE_SCHEDULE_DIST_DBG) {
        //     print_dbg("N=%d, Expansion continues at NT=%llu\n", ctx.getNetworkId(), NetworkTime::now().get());
        // }

        expandSchedule(schedule);
        
        if (explicitScheduleComplete) { // schedule expansion can now be finished

            if(ENABLE_SCHEDULE_DIST_DBG) {
                print_dbg("[SD] N=%d, expandSchedule: allocated %d buffers\n", ctx.getNetworkId(), buffers.size());
                if(explicitSchedule.size() != scheduleSlots) {
                    print_dbg("[SD] N=%d, BUG: Schedule expansion inconsistency\n", ctx.getNetworkId());
                }
            }

            completeWakeupLists();

            if (ENABLE_SCHEDULE_DIST_DBG) {
                print_dbg("[SD] N=%d, Expansion complete at NT=%llu\n", ctx.getNetworkId(), NetworkTime::now().get());
            }

            // all operations done
            inProgress = false;
        }
    }
}

bool ScheduleExpander::needToContinueExpansion() {
    return !explicitScheduleComplete;
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
    unsigned char nodeID = ctx.getNetworkId();
 
    // if(ENABLE_SCHEDULE_DIST_DBG) {
    //     print_dbg("[SD] N=%d, Expanding schedule at NT=%llu\n", nodeID, NetworkTime::now().get());
    // }

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
            // indicate if stream already added to wakeup lists
            bool streamNotYetInserted = false;
            // only consider the first of the redundant apparisons of stream
            if (uniqueStreams.find(e.getStreamId()) == uniqueStreams.end()) {
                streamNotYetInserted = true;
            }

            bool firstSlot = true;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots)
            {
                explicitSchedule[slot] = ExplicitScheduleElement(action, e.getStreamInfo());
                if(buffer) explicitSchedule[slot].setBuffer(buffer);

                // for those streams of the current node that have to send something,
                // add their info to a table, which will be later used by the StreamWaitScheduler
                if (action == Action::SENDSTREAM) {
                    // only use first apparison of stream in schedule as an offset,
                    // used to later compute the wakeup time of each stream
                    if (streamNotYetInserted && firstSlot) {
                        auto wakeupAdvance = streamMgr->getWakeupAdvance(e.getStreamId());
                        if (wakeupAdvance > 0) { // otherwise no need to wakeup it when requested
                            uniqueStreams.insert(e.getStreamId());
                            // create and add StreamWakeupInfo to stream wakeup list
                            addStreamToWakeupList(e, wakeupAdvance, activationTile);
                        }
                        firstSlot = false;
                    }
                }
            }
        }

        expansionIndex++;
    }
    
    if (expansionIndex == schedule.size()) {
        // Computation of the explicit schedule done
        explicitScheduleComplete = true;
    }
}

void ScheduleExpander::addStreamToWakeupList(const ScheduleElement& stream, unsigned long long wakeupAdvance, unsigned int activationTile) {

    bool negativeSlot = false;

    int wakeupSlot = stream.getOffset() - (wakeupAdvance / ctx.getDataSlotDuration());
    if (wakeupSlot < 0) {
        negativeSlot = true;
        wakeupSlot = slotsInSuperframe + wakeupSlot; // convert to positive (referred to previous tile)
    }

    unsigned int tileIndexInSuperframe = wakeupSlot / slotsInTile;
    unsigned int slackTime =  tileIndexInSuperframe * ctx.getTileSlackTime();
    unsigned int wakeupTimeOffset = (wakeupSlot * ctx.getDataSlotDuration()) + slackTime;
    unsigned long long period = toInt(stream.getPeriod()) * netConfig.getTileDuration(); // period converted to time (ns)

    StreamWakeupInfo swi{WakeupInfoType::STREAM, stream.getStreamId(), wakeupTimeOffset, period};

    // If wakeup slot was negative, add element to the "next" list, otherwise to the "curr" list
    // to maintan separated the wakeup times relative to the next or to the current superframe
    if (negativeSlot) {
        // In this case, the wakeup time offset (wakeup slot) is relative
        // to the previous superframe w.r.t. to the activation one
        // (so the first time this stream will have to be woken up is the start time of the
        // last control superframe before schedule's activation time + the stream's offset)
        auto activationTime = (activationTile - superframeSize) * netConfig.getTileDuration();
        swi.wakeupTime += NetworkTime::fromNetworkTime(activationTime).toLocalTime();
        nextList.emplace_back(swi);
        negativeSlot = false;
    }
    else {
        // The wakeup time offset (wakeup slot) is relative to the activation tile
        // (so the first time this stream will have to be woken up is the schedule
        // activation time + the stream's offset)
        auto activationTime = activationTile * netConfig.getTileDuration();
        swi.wakeupTime += NetworkTime::fromNetworkTime(activationTime).toLocalTime();
        currList.emplace_back(swi);
    }
}

void ScheduleExpander::completeWakeupLists() {
    // make the streams wait scheduler to wakeup on downlink slots
    addDownlinkTimesToWakeupList(currList);

    // sort but keep elements with the same value in the same order
    std::stable_sort(currList.begin(), currList.end());
    std::stable_sort(nextList.begin(), nextList.end());

    streamMgr->setStreamsWakeupLists(currList, nextList);
}

void ScheduleExpander::addDownlinkTimesToWakeupList(std::vector<StreamWakeupInfo>& list) {
    ControlSuperframeStructure superframeStructure = netConfig.getControlSuperframeStructure();
    // Number of data slots that compose a single downlink slot
    unsigned int dataSlotsInDownlink = ctx.getDownlinkSlotDuration() / ctx.getDataSlotDuration();
    // Schedule's activation time, corresponding to activation tile
    auto activationTime = activationTile * netConfig.getTileDuration();
    // each of the downlink slots will be repeated at each superframe
    // i.e. period = number_tiles_in_superframe
    // e.g. if superframe size = 2 => downlink slots period = 2 tile
    unsigned long long downlinkSlotPeriod = superframeSize * netConfig.getTileDuration(); // period converted to time (ns)
    for (int i = 0; i < superframeSize; i++) {
        if (superframeStructure.isControlDownlink(i)) {
            // compute downlink tile wakeup offset, wakeup at the end of the downlink slots
            unsigned int downlinkWakeupSlot = (i * slotsInTile) + dataSlotsInDownlink;
            unsigned int downlinkWakeupTimeOffset = downlinkWakeupSlot * ctx.getDataSlotDuration();
            // the wakeup time offset (wakeup slot) is relative to the activation tile
            unsigned long long downlinkWakeupTime = downlinkWakeupTimeOffset + NetworkTime::fromNetworkTime(activationTime).toLocalTime();
            list.emplace_back(StreamWakeupInfo{WakeupInfoType::DOWNLINK, StreamId(), downlinkWakeupTime, downlinkSlotPeriod});
        }
    }
}

std::pair<unsigned int, unsigned int> ScheduleExpander::countStreams(const std::vector<ScheduleElement>& schedule) {
    // Implicit schedule contains repeated streams, according to their
    // redundancy value, keep track of already counted streams
    std::set<StreamId> countedStreams;

    unsigned char nodeID = ctx.getNetworkId();
    unsigned int numNegativeOffset = 0;
    unsigned int numTotal = 0;

    for (auto e : schedule) {
        if (countedStreams.find(e.getStreamId()) == countedStreams.end()) {
            // count only this node's streams that have to send
            if (e.getSrc() == nodeID && e.getTx() == nodeID) {
                numTotal++;
                auto wakeupAdvance = streamMgr->getWakeupAdvance(e.getStreamId());
                int wakeupSlot = e.getOffset() - (wakeupAdvance / ctx.getDataSlotDuration());
                if (wakeupSlot < 0) {
                    numNegativeOffset++;
                }
            }

            countedStreams.insert(e.getStreamId());
        }
    }

    return std::make_pair(numTotal, numNegativeOffset);
}

}; // namespace mxnet
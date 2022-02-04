/***************************************************************************
 *   Copyright (C) 2018-2022 by Federico Amedeo Izzo, Valeria Mazzola,     *
 *                              Luca Conterio                              *
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

ScheduleExpander::ScheduleExpander(MACContext& c) : ctx(c), streamMgr(c.getStreamManager()) {}

void ScheduleExpander::expandSchedule(const std::vector<ScheduleElement>& schedule, 
                                                                        const ScheduleHeader& header, unsigned char nodeID)
{
    // New explicitSchedule to return
    std::vector<ExplicitScheduleElement> result;
    std::map<unsigned int,std::shared_ptr<Packet>> buffers;
    forwardedStreamCtr = std::map<StreamId, std::pair<unsigned char, unsigned char>>();
    // Resize new explicitSchedule and fill with default value (sleep)
    auto slotsInTile = ctx.getSlotsInTileCount();
    auto scheduleSlots = header.getScheduleTiles() * slotsInTile;
    result.resize(scheduleSlots, ExplicitScheduleElement());

    std::map<unsigned int, StreamOffsetInfo> streamOffsetsTable;

    // Scan implicit schedule for element that imply the node action
    for(auto e : schedule)
    {
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
                if(ENABLE_SCHEDULE_DIST_DBG)
                    print_dbg("[SD] Error: expandSchedule missing buffer\n");
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
            unsigned int offset = 0;
            bool streamNotYetInserted = false;
            unsigned int wakeupAdvance = 0;
            
            // only consider first of the redundant apparisons of stream
            if (streamOffsetsTable.count(e.getStreamId().getKey()) == 0) {
                streamNotYetInserted = true;
            }

            bool firstSlot = true;
            bool offsetSet = false;
            for(auto slot = e.getOffset(); slot < scheduleSlots; slot += periodSlots)
            {
                result[slot] = ExplicitScheduleElement(action, e.getStreamInfo());
                if(buffer) result[slot].setBuffer(buffer);

                // for those streams of the current node that have to send something,
                // add their info to a table, which will be later used by the StreamWaitScheduler
                if (action == Action::SENDSTREAM) {
                    // only use first apparison of stream in schedule as an offset,
                    // used to later compute the wakeup time of each stream
                    if (streamNotYetInserted && firstSlot) {
                        wakeupAdvance = streamMgr->getWakeupAdvance(e.getStreamId());
                        if (wakeupAdvance > 0) { // otherwise no need to wakeup it when requested
                            offset = slot;
                            offsetSet = true;
                        }
                        firstSlot = false;
                    }
                }
            }

            if (streamNotYetInserted && offsetSet) {
                StreamOffsetInfo swi{e.getStreamId(), toInt(e.getParams().getPeriod()), offset, wakeupAdvance};
                streamOffsetsTable.insert({e.getStreamId().getKey(), swi});
            }
        }
    }
    
    if(ENABLE_SCHEDULE_DIST_DBG)
    {
        print_dbg("[SD] expandSchedule: allocated %d buffers\n", buffers.size());
        if(result.size() != scheduleSlots)
            print_dbg("[SD] BUG: Schedule expansion inconsistency\n");
    }

    if(ENABLE_SCHEDULE_DIST_MAS_INFO_DBG) {
        printStreamsInfoTable(streamOffsetsTable, nodeID);
    }

    computeStreamsWakeupLists(streamOffsetsTable, header);

    explicitSchedule = result;
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

void ScheduleExpander::computeStreamsWakeupLists(const std::map<unsigned int, StreamOffsetInfo>& table, const ScheduleHeader& header) {
    const NetworkConfiguration& netConfig = ctx.getNetworkConfig();

    std::pair<unsigned int, unsigned int> p = countStreamsWithNegativeOffset(table);
    unsigned int numStreamsWithNegativeOffset = p.first;
    unsigned int numScheduledStreams = p.second;

    std::vector<StreamWakeupInfo> currList;
    currList.reserve(numScheduledStreams - numStreamsWithNegativeOffset
                            + netConfig.getControlSuperframeStructure().countDownlinkSlots());
    std::vector<StreamWakeupInfo> nextList;
    nextList.reserve(numStreamsWithNegativeOffset);

    unsigned int numSlotsInSuperframe = netConfig.getControlSuperframeStructure().size() * 
                                        ctx.getSlotsInTileCount();

    bool negativeSlot = false;

    for (auto it = table.begin(); it != table.end(); it++) {
        auto stream = it->second;

        int wakeupSlot = stream.offset - (stream.wakeupAdvance / ctx.getDataSlotDuration());
        if (wakeupSlot < 0) {
            negativeSlot = true;
            wakeupSlot = numSlotsInSuperframe + wakeupSlot; // convert to positive (referred to previous tile)
        }

        unsigned int tileIndexInSuperframe = wakeupSlot / ctx.getSlotsInTileCount();
        unsigned int slackTime =  tileIndexInSuperframe * ctx.getTileSlackTime();
        unsigned int wakeupTimeOffset = (wakeupSlot * ctx.getDataSlotDuration()) + slackTime;

        StreamWakeupInfo swi{WakeupInfoType::STREAM, stream.id, wakeupTimeOffset, stream.period};

        // if wakeup slot was negative, add element to one list, otherwise to the other
        // to maintan separated the wakeup times relative to the current or to the next superframe
        if (negativeSlot) {
            // in this case, the wakeup time offset (wakeup slot) is relative
            // to the previous superframe w.r.t. to the activation one
            auto tilesAdvance = netConfig.getControlSuperframeStructure().size();
            auto baseTime = (header.getActivationTile() - tilesAdvance) * ctx.getNetworkConfig().getTileDuration();
            swi.wakeupTime += NetworkTime::fromNetworkTime(baseTime).toLocalTime();
            nextList.emplace_back(swi);
            negativeSlot = false;
        }
        else {
            // the wakeup time offset (wakeup slot) is relative to the activation tile
            auto baseTime = header.getActivationTile() * ctx.getNetworkConfig().getTileDuration();
            swi.wakeupTime += NetworkTime::fromNetworkTime(baseTime).toLocalTime();
            currList.emplace_back(swi);
        }
    }

    // make the streams wait scheduler to wakeup on downlink slots
    addDownlinkTimesToWakeupList(currList, header); 
    
    // sort but keep elements with the same value in the same order
    std::stable_sort(currList.begin(), currList.end());
    std::stable_sort(nextList.begin(), nextList.end());

    streamMgr->setStreamsWakeupLists(currList, nextList);
}

std::pair<unsigned int, unsigned int> ScheduleExpander::countStreamsWithNegativeOffset(const std::map<unsigned int, StreamOffsetInfo>& table) {
    unsigned int countNegativeOffsets = 0;
    unsigned int countScheduledStreams = 0;

    for (auto it = table.begin(); it != table.end(); it++) {
        countScheduledStreams++;
        auto off = it->second.offset;
        auto wakeupAdvance = it->second.wakeupAdvance;
        int wakeupSlot = off - (wakeupAdvance / ctx.getDataSlotDuration());
        if (wakeupSlot < 0) {
            countNegativeOffsets++;
        }
    }

    return std::make_pair(countNegativeOffsets, countScheduledStreams);
}

void ScheduleExpander::addDownlinkTimesToWakeupList(std::vector<StreamWakeupInfo>& list, const ScheduleHeader& header) {
    ControlSuperframeStructure superframeStructure = ctx.getNetworkConfig().getControlSuperframeStructure();
    unsigned int numConsecutiveDownlinkSlots = ctx.getDownlinkSlotDuration() / ctx.getDataSlotDuration();
    // each of the downlink slots will be repeated at each superframe
    // i.e. period = number_tiles_in_superframe
    // e.g. if superframe size = 2 => downlink slots period = 2 tile
    int downlinkSlotPeriod = superframeStructure.size();
    for (int i = 0; i < superframeStructure.size(); i++) {
        if (superframeStructure.isControlDownlink(i)) {
            // compute downlink tile wakeup offset, wakeup at the end of the downlink slots
            unsigned int downlinkWakeupSlot = (i * ctx.getSlotsInTileCount()) + numConsecutiveDownlinkSlots;
            unsigned int downlinkWakeupTimeOffset = downlinkWakeupSlot * ctx.getDataSlotDuration();
            // the wakeup time offset (wakeup slot) is relative to the activation tile
            unsigned long long downlinkWakeupTime = downlinkWakeupTimeOffset + (header.getActivationTile() * ctx.getNetworkConfig().getTileDuration());
            list.emplace_back(StreamWakeupInfo{WakeupInfoType::DOWNLINK, StreamId(), downlinkWakeupTime, downlinkSlotPeriod});
        }
    }
}

void ScheduleExpander::printStreamsInfoTable(const std::map<unsigned int, StreamOffsetInfo>& table, unsigned char nodeID) const {
#ifndef _MIOSIX
    if (table.size() == 0) {
        print_dbg("[SD] Node: %d, streams info table is empty\n", nodeID);
        return;
    }
    
    print_dbg("[SD] Node: %d, streams info table\n", nodeID);
    print_dbg("ID \t\tPER \tOffset \tWakeupAdvance \n");
    
    for (auto it = table.begin(); it != table.end(); it++) {
        print_dbg("%lu \t%d \t%u \t%u \n", it->second.id.getKey(), it->second.period, it->second.offset, it->second.wakeupAdvance);
    }
#endif
}

}; // namespace mxnet
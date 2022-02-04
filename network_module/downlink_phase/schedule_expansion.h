/***************************************************************************
 *   Copyright (C) 2018-2022 by Federico Amedeo Izzo, Valeria Mazzola,     *
 *                              Luca Conterio                              *
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

#pragma once

#include "../mac_context.h"
#include "../scheduler/schedule_element.h"
#include "../stream/stream_wait_data.h"
#include <vector>

namespace mxnet {

class ScheduleExpander {

public:
    ScheduleExpander(MACContext& c);

    /**
     * Convert the implicit schedule to an explicit one
     * \param schedule
     * \param header
     */
    void expandSchedule(const std::vector<ScheduleElement>& schedule, const ScheduleHeader& header, unsigned char nodeID);

    const std::vector<ExplicitScheduleElement>& getExplicitSchedule();

    unsigned int getExpansionsPerSlot() const;

    const std::map<StreamId, std::pair<unsigned char, unsigned char>>& getForwardedStreams() const;

private:
    /**
     * Create two lists of streams and pass them to the StreamManager.
     * The two lists contain the streams with their respective wakeup times, divided
     * in streams that have to transmit in the current superframe and streams that have
     * to transmit in the next superframe.
     * \param table the list of all the streams that have to transmit togheter with their
     *              main params (period, redundancy and wakeup advance)
     * \param header
     */
    void computeStreamsWakeupLists(const std::map<unsigned int, StreamOffsetInfo>& table, const ScheduleHeader& header);

    /**
     * \param table the map of all the streams that have to transmit togheter with their
     *              main params (period, redundancy and wakeup advance)
     * \return the number of streams whose wakeup time is contained in the previous superframe
     *         (w.r.t. the transmission superframe) and the total number of scheduled streams.
     */
    std::pair<unsigned int, unsigned int> countStreamsWithNegativeOffset(const std::map<unsigned int, StreamOffsetInfo>& table);
    
    /**
     * Insert downlink slots end times to the given list.
     * \param list the list to which downlink slots times have to be added
     */
    void addDownlinkTimesToWakeupList(std::vector<StreamWakeupInfo>& list, const ScheduleHeader& header);

    /**
     * 
     */
    void printStreamsInfoTable(const std::map<unsigned int, StreamOffsetInfo>& table, unsigned char nodeID) const;

    std::vector<ExplicitScheduleElement> explicitSchedule;

    /* Structure used to keep count of redundancy groups of streams that this
     * node is scheduled to forward to others. This info will be moved to 
     * dataphase at the end of schedule explicitation and used to correctly
     * clear the buffers.
     * For each stream being forwarded, the second number of the pair is set
     * to the number of transmissions that this node is assigned. The first 
     * number of the pair will be set to zero and used as counter by
     * the dataphase */
    std::map<StreamId, std::pair<unsigned char, unsigned char>> forwardedStreamCtr;

    const MACContext& ctx;
    StreamManager* const streamMgr;

    // TODO measure this
    const unsigned int expansionsPerSlot = 15;
};

}; // namespace mxnet
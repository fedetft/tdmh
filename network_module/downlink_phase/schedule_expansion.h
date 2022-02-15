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

#pragma once

#include "../mac_context.h"
#include "../scheduler/schedule_element.h"
#include "../stream/stream_wait_data.h"
#include <vector>

namespace mxnet {

/**
 * Class that is in charge of transforming an implict schedule
 * to an explicit one (expansion process).
 * The expansion can be executed over multiple downlink slots.
 */
class ScheduleExpander {

public:
    ScheduleExpander(MACContext& c);

    /**
     * Called directly by ScheduleDistribution phase when a schedule is received.
     */
    void startExpansion(const std::vector<ScheduleElement>& schedule, const ScheduleHeader& header);

    /**
     * Called directly by ScheduleDistribution phase in downlink tiles reserved for
     * expansion.
     */
    void continueExpansion(const std::vector<ScheduleElement>& schedule);

    /**
     * Called by ScheduleDistribution to know when there are no more streams to expand.
     * \return true if there are streams that need to be expanded and expansion is in progress,
     *         false otherwise.
     */
    bool needToContinueExpansion();

    /**
     * \return the reference to the explicit (expanded) schedule
     */
    const std::vector<ExplicitScheduleElement>& getExplicitSchedule();

    /**
     * \return the number of streams that can be expanded in a single downlink slot
     */
    unsigned int getExpansionsPerSlot() const;

    /**
     * \return 
     */
    const std::map<StreamId, std::pair<unsigned char, unsigned char>>& getForwardedStreams() const;

private:
    /**
     * Convert the implicit schedule to an explicit one
     * \param schedule
     */
    void expandSchedule(const std::vector<ScheduleElement>& schedule);

    /**
     * Create two lists of streams and pass them to the StreamManager.
     * The two lists contain the streams with their respective wakeup times, divided
     * in streams that have to transmit in the current superframe and streams that have
     * to transmit in the next superframe.
     * \param stream
     * \param wakeupAdvance
     * \param activationTile
     */
    void addStreamToWakeupList(const ScheduleElement& stream, unsigned long long wakeupAdvance, unsigned int activationTile);
    
    /**
     * 
     */
    void completeWakeupLists();

    /**
     * Insert downlink slots end times to the given list.
     * \param list the list to which downlink slots times have to be added
     */
    void addDownlinkTimesToWakeupList(std::vector<StreamWakeupInfo>& list);

    /**
     * \return the total number of scheduled streams and the number of streams whose wakeup time is 
     *         contained in the previous superframe (w.r.t. the transmission superframe).
     */
    std::pair<unsigned int, unsigned int> countStreams(const std::vector<ScheduleElement>& schedule);

    // Index used to iterate over the implicit schedule
    unsigned int expansionIndex = 0;

    // Number of slots included in explicit schedule
    // i.e. explicit schedule vector size
    unsigned int scheduleSlots = 0;
    // Number of slots in a single tile
    unsigned int slotsInTile = 0;
    // Number of slots in an entire control superframe
    unsigned int slotsInSuperframe = 0;
    // Number of tiles in a single control superframe
    int superframeSize = 0;

    // Received schedule activation tile
    unsigned int activationTile = 0;

    // Indicate if the process is still ongoing or not
    bool inProgress = false;
    // Indicate if the expansion process reached
    // the end of the implicit schedule vector
    bool explicitScheduleComplete = false;

    // Explicit schedule resulting after the expansion is complete
    std::vector<ExplicitScheduleElement> explicitSchedule;

    // 
    std::map<unsigned int, std::shared_ptr<Packet>> buffers;

    // Keep track of which streams have already been added to streams wakeup lists
    // (since only unique streams have to be added, without considering their redundancy)
    std::set<StreamId> uniqueStreams;

    /* Structure used to keep count of redundancy groups of streams that this
     * node is scheduled to forward to others. This info will be moved to 
     * dataphase at the end of schedule explicitation and used to correctly
     * clear the buffers.
     * For each stream being forwarded, the second number of the pair is set
     * to the number of transmissions that this node is assigned. The first 
     * number of the pair will be set to zero and used as counter by
     * the dataphase */
    std::map<StreamId, std::pair<unsigned char, unsigned char>> forwardedStreamCtr;

    // Streams wakeup lists, needed by the StreamWaitScheduler
    std::vector<StreamWakeupInfo> currList;
    std::vector<StreamWakeupInfo> nextList;

    const MACContext& ctx;
    const NetworkConfiguration& netConfig;
    StreamManager* const streamMgr;

    //
    // TODO measure these
    //

    // Maximum number of elements in the inmplicit schedule
    // that can be expanded during a downlink slot
    unsigned int expansionsPerSlot   = 0;
    // Time needed to get an element from the implicit schedule
    // and insert it into the explicit one, in nanoseconds
    // Set to worst case, i.e. action is SENDSTREAM / SENDBUFFER
    const unsigned long long singleExpansionTime = 32000 + 1000; // 32 us + slack (1 us)

    // addDownlinkTimesToWakeupList() with one downlink = 5.87 us
    // single downlink slot add to list = 2 us 
    // ===> 5.87 - 2 ~= 3.9 us
    const unsigned long long addDownlinkPreparationTime  = 3900;
    const unsigned long long addDownlinkSingleTime = 2000 + 1000; // 2 us + slack (1 us)

    // total time for schedule with 1 stream and redundancy 3 = 32.625 us
    // (including creating the needed objects, e.g. the set where seen streams are added)
    // time for checking if stream exists to the "set" and adding it to the "set" = 20.6 us ~= 21 us
    const unsigned long long countStreamsSingleTime = 21000 + 1000; // 21 us + slack (1 us)

    std::mutex expansionMutex;
};

}; // namespace mxnet
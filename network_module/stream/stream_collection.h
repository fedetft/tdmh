/***************************************************************************
 *   Copyright (C)  2019 by Federico Amedeo Izzo                           *
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

// For StreamId, StreamStatus, StreamManagementElement
#include "stream_management_element.h"
// For InfoElement
#include "../scheduler/schedule_element.h"
// For ReceiveUplinkMessage
#include "../uplink_phase/uplink_message.h"
#include <map>

namespace mxnet {

/**
 * The class StreamCollection represents the status of all the Streams and Servers
 * in the network.
 * If a schedule is being distributed, the status corresponds the next schedule.
 * It is used by ScheduleComputation
 */
class StreamCollection {
    friend class StreamSnapshot;
public:
    StreamCollection() {};
    ~StreamCollection() {};
    /**
     * Receives a vector of SME from the network
     */
    void receiveSMEs(UpdatableQueue<StreamId,
                     StreamManagementElement>& smes);
    /**
     * Receives a schedule form the scheduler
     */
    void receiveSchedule(const std::list<ScheduleElement>& schedule);
    /**
     * @return the number of Info elements in queue
     */
    unsigned int getNumInfo() {
        return infoQueue.size();
    }
    /**
     * @return vector containing all the streams
     */
    std::vector<MasterStreamInfo> getStreams();
    /**
     * put a new info element in the queue
     */
    void enqueueInfo(StreamId id, InfoType type);
    /**
     * pops a number of Info elements from the Queue
     */
    std::vector<InfoElement> dequeueInfo(unsigned int num);
    /**
     * @return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const {
        return modified_flag;
    };

private:
    /**
     * Reset all the flags to false 
     */
    void clearFlags() {
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    }
    /**
     * Called by receiveSMEs(), used to update the status of the Stream 
     */
    void updateStream(MasterStreamInfo& stream, StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to update the status of the Server
     */
    void updateServer(MasterStreamInfo& server, StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Stream in collection 
     */
    void createStream(StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Server in collection
     */
    void createServer(StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Server in collection
     */
    StreamParameters negotiateParameters(StreamParameters& serverParams, StreamParameters& clientParams);

    /* Map containing information about all Streams and Server in the network */
    std::map<StreamId, MasterStreamInfo> collection;
    /* UpdatableQueue of Info elements to send to the network */
    UpdatableQueue<StreamId, InfoElement> infoQueue;
    /* Flags that record the changes to the Streams */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

/**
 * The class StreamSnapshot represents a snapshot copy of the StreamCollection used
 * by the scheduler.
 * It has no concurrency control because it is accessed exclusively by the scheduler,
 * and finally it takes care of preparing a list of changes to apply to the
 * real StreamCollection.
 */
class StreamSnapshot {
public:
    StreamSnapshot() {};
    ~StreamSnapshot() {};
    /**
     * Updates a StreamSnapshot with the content of a StreamCollection.
     * NOTE: this is different from a copy constructor for two reasons:
     * - because it does not copy the infoQueue, which is not needed by the scheduler
     * - because it clears the flags of the passed StreamCollection to detect changes
     */
    void update(StreamCollection& other) {
        collection = other.collection;
        modified_flag = other.modified_flag;
        removed_flag = other.removed_flag;
        added_flag = other.added_flag;
        other.clearFlags();
    }
    /**
     * @return the number of Streams saved
     */
    unsigned int getStreamNumber() {
        return collection.size();
    }
    /**
     * @return vector containing all the streams
     */
    std::vector<MasterStreamInfo> getStreams();
    /**
     * @return a vector of MasterStreamInfo that matches the given status
     */
    std::vector<MasterStreamInfo> getStreamsWithStatus(MasterStreamStatus s);
    /**
     * @return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const {
        return modified_flag;
    };
    /**
     * @return true if any stream was removed since last time the flag was cleared 
     */
    bool wasRemoved() const {
        return removed_flag;
    };
    /**
     * @return true if any stream was added since last time the flag was cleared 
     */
    bool wasAdded() const {
        return added_flag;
    };

private:
    /* Map containing information about all Streams and Server in the network */
    std::map<StreamId, MasterStreamInfo> collection;
    /* Flags that record the changes to the Streams */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

} // namespace mxnet

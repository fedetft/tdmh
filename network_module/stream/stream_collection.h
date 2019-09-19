/***************************************************************************
 *   Copyright (C) 2019 by Federico Amedeo Izzo                            *
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
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
#endif

namespace mxnet {

enum class StreamChange
{
    ESTABLISH,       // For ACCEPTED streams in snapshot, present in new schedule
    REJECT,          // For ACCEPTED streams in snapshot, missing from new schedule
    CLOSE,           // For ESTABLISHED streams in snapshot, missing from new schedule
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
    StreamSnapshot(std::map<StreamId, MasterStreamInfo> collection, bool modified,
                   bool removed, bool added) : collection(collection),
                                               modified_flag(modified),
                                               removed_flag(removed),
                                               added_flag(added) {}
    ~StreamSnapshot() {};
    /**
     * @return the number of Streams saved
     */
    unsigned int getStreamNumber() const {
        return collection.size();
    }
    /**
     * @return vector containing all the streams
     */
    std::vector<MasterStreamInfo> getStreams() const;
    /**
     * @return a vector of MasterStreamInfo that matches the given status
     */
    std::vector<MasterStreamInfo> getStreamsWithStatus(MasterStreamStatus s) const;
    /**
     * @return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const {
        return modified_flag;
    }
    /**
     * @return true if any stream was removed since last time the flag was cleared 
     */
    bool wasRemoved() const {
        return removed_flag;
    }
    /**
     * @return true if any stream was added since last time the flag was cleared 
     */
    bool wasAdded() const {
        return added_flag;
    }
    /**
     * @return a map containing changes to apply on the StreamCollection,
     * based on the comparison of schedule with StreamSnapshot.
     * The map has streamId as key and StreamChange as value, which is an enum containing
     * different changes to apply on streams, for example establish, reject or close.
     */
    std::map<StreamId, StreamChange> getStreamChanges(const std::list<ScheduleElement>& schedule) const;

private:
    /* Map containing information about all Streams and Server in the network */
    std::map<StreamId, MasterStreamInfo> collection;
    /* Flags that record the changes to the Streams */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

/**
 * The class StreamCollection represents the status of all the Streams and Servers
 * in the network.
 * If a schedule is being distributed, the status corresponds the next schedule.
 * It is used by ScheduleComputation
 */

class StreamCollection {
public:
    StreamCollection() {};
    ~StreamCollection() {};
    /**
     * Receives a vector of SME from the network
     */
    void receiveSMEs(UpdatableQueue<StreamId,
                     StreamManagementElement>& smes);
    /**
     * Apply changes precomputed by StreamSnaphot::getStreamChanges()
     */
    void applyChanges(const std::map<StreamId, StreamChange>& changes);
    /**
     * @return vector containing all the streams
     */
    std::vector<MasterStreamInfo> getStreams();
    /**
     * @return the number of Info elements in queue
     */
    unsigned int getNumInfo() {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(coll_mutex);
#else
        std::unique_lock<std::mutex> lck(coll_mutex);
#endif
        return infoQueue.size();
    }
    /**
     * pops a number of Info elements from the Queue
     */
    std::vector<InfoElement> dequeueInfo(unsigned int num);
    /**
     * @return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(coll_mutex);
#else
        std::unique_lock<std::mutex> lck(coll_mutex);
#endif
        return modified_flag;
    }
    /**
     * Returns a StreamSnapshot containing a snapshot of a StreamCollection.
     * NOTE: this is different from a copy constructor for two reasons:
     * - because it does not copy the infoQueue, which is not needed by the scheduler
     * - because it clears the flags of the passed StreamCollection to detect changes
     */
    StreamSnapshot getSnapshot() {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(coll_mutex);
#else
        std::unique_lock<std::mutex> lck(coll_mutex);
#endif
        StreamSnapshot result(collection, modified_flag, removed_flag, added_flag);
        clearFlags();
        return result;
    }

private:
    /**
     * Reset all the flags to false 
     * NOTE: called with mutex already locked
     */
    void clearFlags() {
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    }
    /**
     * put a new info element in the queue
     * NOTE: called with mutex already locked
     */
    void enqueueInfo(StreamId id, InfoType type);
    /**
     * Called by receiveSMEs(), used to update the status of the Stream 
     * NOTE: called with mutex already locked
     */
    void updateStream(MasterStreamInfo& stream, StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to update the status of the Server
     * NOTE: called with mutex already locked
     */
    void updateServer(MasterStreamInfo& server, StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Stream in collection 
     * NOTE: called with mutex already locked
     */
    void createStream(StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Server in collection
     * NOTE: called with mutex already locked
     */
    void createServer(StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to create a new Server in collection
     * NOTE: called with mutex already locked
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

    /* Mutex to protect concurrect access at collection and infoQueue
     * from the TDMH thread and the scheduler thread */
#ifdef _MIOSIX
    miosix::Mutex coll_mutex;
#else
    std::mutex coll_mutex;
#endif

};

} // namespace mxnet

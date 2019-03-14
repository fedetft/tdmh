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

// For StreamId
#include "uplink/stream_management/stream_management_element.h"
#include "stream.h"
#include "schedule/schedule_element.h"
// For thread synchronization
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
#endif
#include <map>
#include <queue>

namespace mxnet {

/**
 * The class StreamCollection represents a snapshot of the streams on the network
 * takend before starting the schedule computation
 * It is used by ScheduleComputation
 */
class StreamCollection {
public:
    StreamCollection() {};
    StreamCollection(std::map<StreamId, StreamInfo> map, bool modified,
                     bool removed, bool added) : collection(map),
                                                 modified_flag(modified),
                                                 removed_flag(removed),
                                                 added_flag(added) {}
    ~StreamCollection() {};

    /**
     * @return the number of Streams saved
     */
    unsigned char getStreamNumber() { return collection.size(); }
    /**
     * get the status of a given Stream
     */
    StreamStatus getStreamStatus(StreamId id);
    /**
     * change the state of the saved Stream
     */
    void setStreamStatus(StreamId id, StreamStatus status);
    /**
     * @return vector containing all the streams
     */
    std::vector<StreamInfo> getStreams();
    /**
     * @return a vector of StreamInfo that matches the given status
     */
    std::vector<StreamInfo> getStreamsWithStatus(StreamStatus s);
    /**
     * @return true if there are streams not yet scheduled
     */
    bool hasSchedulableStreams();
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
    void clear() {
        collection.clear();
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    }
    void swap(StreamCollection& rhs) {
        collection.swap(rhs.collection);
        std::swap(modified_flag, rhs.modified_flag);
        std::swap(removed_flag, rhs.removed_flag);
        std::swap(added_flag, rhs.added_flag);
    }

private:
    /* Map containing information about Streams related to this node */
    std::map<StreamId, StreamInfo> collection;
    /* Snapshot copy of the bits in StreamManager */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

/**
 * The class StreamManager contains pointers to all the Stream classes
 * created in the current node.
 * It is used by classes UplinkPhase, DataPhase
 * NOTE: the methods of this class are protected by a mutex,
 * Do not call one method from another! or you will get a deadlock.
 */

class StreamManager {
public:
    StreamManager(unsigned char id) : myId(id) {};
    ~StreamManager() {
        closeAllStreams();
    }

    // Used by StreamManager destructor and when TDMH is shutting down
    void closeAllStreams();
    // Used by Scheduler when receiving StreamServer closing SME to close related streams
    void closeStreamsRelatedToServer(StreamId id);
    // Used by the Stream class to register itself in the Stream Map
    void registerStream(StreamInfo info, Stream* client);
    // Used by the Stream class to get removed from the Stream Map
    void deregisterStream(StreamInfo info);
    // Used by the StreamServer class to register itself in the Server Map
    void registerStreamServer(StreamInfo info, StreamServer* server);
    // Used by the StreamServer class to get removed from the Stream Map
    void deregisterStreamServer(StreamInfo info);
    /** Used to update the status of the Stream and wake up the corresponding thread
     *  when receiving a new schedule containing that Stream.
     *  used by the DataPhase called by ScheduleDownlink
     */
    void notifyStreams(const std::vector<ScheduleElement>& schedule);
    // Used by the DataPhase to put/get data to/from buffers
    void putBuffer(StreamId id, Packet& pkt) {
        // NOTE: call getStreamStatus before mutex to avoid a deadlock
        if(getStreamStatus(id) == StreamStatus::CLOSED) {
            print_dbg("DataPhase::receiveToStream: Stream (%d,%d) is CLOSED",
                      id.src, id.dst);
            return;
        }
        // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
        std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
        if (clientMap.find(id) == clientMap.end())
            throwRuntimeError("DataPhase::receiveToStream: stream (%d,%d) is not registered in node %d",
                              id.src, id.dst, myId);
        clientMap[id]->putRecvBuffer(pkt);
    }
    Packet getBuffer(StreamId id) {
        // NOTE: call getStreamStatus before mutex to avoid a deadlock
        if(getStreamStatus(id) == StreamStatus::CLOSED) {
            print_dbg("DataPhase::sendFromStream: Stream (%d,%d) is CLOSED",
                      id.src, id.dst);
            Packet empty;
            return empty;
        }
        // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
        std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
        if (clientMap.find(id) == clientMap.end())
            throwRuntimeError("DataPhase::sendFromStream: stream (%d,%d) is not registered in node %d",
                              id.src, id.dst, myId);
        return clientMap[id]->getSendBuffer();
    }
    /**
     * @return the number of Streams saved, not counting LISTEN requests
     */
    unsigned char getStreamNumber();
    /**
     * @return the state of the saved Stream
     */
    StreamStatus getStreamStatus(StreamId id);
    /**
     * Change the state of the saved Stream
     */
    void setStreamStatus(StreamId id, StreamStatus status);
    /**
     * @return the parameters of the saved Stream
     */
    StreamInfo getStreamInfo(StreamId id) {
        return streamMap[id];
    }
    /**
     * Register in the Stream Map a single stream 
     */
    void addStream(const StreamInfo& stream);
    /**
     * @return a snapshot of streamMap 
     */
    StreamCollection getSnapshot();
    /**
     * @return the number of SME stored in the Queue
     */
    unsigned char getNumSME();
    /**
     * @return a number of element from the SME queue to send on the network,
     * used by UplinkPhase
     */
    std::vector<StreamManagementElement> dequeueSMEs(unsigned char count);
    /**
     * Enqueue a list of sme received from other nodes, to be forwarded
     * towards the master node.
     * used by UplinkPhase
     */
    void enqueueSMEs(std::vector<StreamManagementElement> smes);
    /**
     * @return the number of Info elements stored in the Queue
     */
    unsigned char getNumInfo();
    /**
     * @return a number of element from the Info element queue to send on the network,
     * used by ScheduleDistribution
     */
    std::vector<InfoElement> dequeueInfo(unsigned char count);
    /**
     * Enqueue a list of Info elements received from other nodes, to be forwarded
     * to the rest of the network.
     * used by ScheduleDistribution
     */
    void enqueueInfo(std::vector<InfoElement> infos);
    /**
     * Consume the Info elements present in the queue and use the relevant information 
     */
    void receiveInfo();
    /**
     * @return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const{
        // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
        std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
        return modified_flag;
    };
    /**
     * Set all flags to false
     */
    void clearFlags() {
        // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
        std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    };

protected:
    /* NetworkId of this node */
    unsigned char myId;
    /* Map containing pointers to StreamServer classes in this node */
    std::map<StreamId, StreamServer*> serverMap;
    /* Map containing pointers to Stream classes in this node */
    std::map<StreamId, Stream*> clientMap;
    /* Map containing information about Streams and StreamServers related to this node */
    std::map<StreamId, StreamInfo> streamMap;
    /* FIFO queue of SME to send from the nodes to the master */
    std::queue<StreamManagementElement> smeQueue;
    /* FIFO queue of Info elements to send from the master to the nodes */
    std::queue<InfoElement> infoQueue;
    /* Thread synchronization */
#ifdef _MIOSIX
    mutable miosix::Mutex streamMgr_mutex;
#else
    mutable std::mutex streamMgr_mutex;
#endif
    /* Flags used by the master node to get whether the streams were changed
       IMPORTANT: this bit must be set to true whenever the data structure is modified */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};


} /* namespace mxnet */

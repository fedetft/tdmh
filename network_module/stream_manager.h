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
    StreamCollection(std::map<StreamId, StreamInfo> map) {
        collection = map;
    };
    ~StreamCollection() {};

    /**
     * @return the number of Streams saved
     */
    unsigned char getStreamNumber() { return collection.size(); }
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

private:
    /* Map containing information about Streams related to this node */
    std::map<StreamId, StreamInfo> collection;
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
    StreamManager() {};
    ~StreamManager() {};

    // Used by the Stream class to register itself in the Stream Map
    void registerStream(StreamInfo info, Stream* stream);
    // Used by the Stream class to get removed from the Stream Map
    void deregisterStream(StreamInfo info);
    // Used by the DataPhase to put/get data to/from buffers
    void putBuffer(unsigned int DstPort, Packet& pkt) {
        //recvbuffer[DstPort] = new Packet(pkt);
    }
    Packet getBuffer(unsigned int SrcPort) {
        Packet buffer;// = stream[SrcPort].getBuffer();
        //Packet buffer = stream[SrcPort].getBuffer();
        //stream[SrcPort].clearBuffer();
        return buffer;
    }
    /**
     * @return the number of Streams saved
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
     * Register in the Stream Map a single stream 
     */
    void addStream(const StreamInfo& stream);
    /**
     * @return a snapshot of streamMap 
     */
    StreamCollection getSnapshot();
    /**
     * @return the number of available SME to send
     * = number of StreamInfo with status LISTEN_REQ or CONNECT_REQ
     */
    unsigned char getNumSME();
    /**
     * @return a number of element from the SME queue to send on the network,
     * used by UplinkPhase
     */
    std::vector<StreamManagementElement> dequeueSMEs(unsigned char count);
    /**
     * Enqueue a list of sme received from other noded, to be forwarded
     * towards the master node.
     * used by UplinkPhase
     */
    void enqueueSMEs(std::vector<StreamManagementElement> smes);

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
    /**
     * Set all flags to false
     */
    void clearFlags() {
        modified_flag = false;
        removed_flag = false;
        added_flag = false;
    };

protected:
    /* Map containing pointers to StreamServer classes in this node */
    //TODO make this a map<StreamId, StreamServer*
    std::map<StreamId, Stream*> serverMap;
    /* Map containing pointers to Stream classes in this node */
    std::map<StreamId, Stream*> clientMap;
    /* Map containing information about Streams related to this node */
    std::map<StreamId, StreamInfo> streamMap;
    /* FIFO queue of SME to send on the network */
    std::queue<StreamManagementElement> smeQueue;
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex stream_mutex;
#else
    std::mutex stream_mutex;
#endif
    /* Flags used by the master node to get whether the streams were changed
       IMPORTANT: this bit must be set to true whenever the data structure is modified */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};


} /* namespace mxnet */

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

// For StreamId, StreamManagementElement
#include "stream_management_element.h"
// For InfoElement
#include "../scheduler/schedule_element.h"
#include <map>

namespace mxnet {

/**
 * The class StreamCollection represents the status of all the Streams and Servers
 * in the network. The status corresponds to after the next schedule will be applied
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
     * Receives a vector of SME from the network
     */
    void receiveSMEs(ReceiveUplinkMessage& msg);
    /**
     * The given stream has been scheduled
     */
    void streamEstablished(StreamId id);
    /**
     * The given stream has not been scheduled
     */
    void streamRejected(StreamId id);
    /**
     * @return the number of Streams saved
     */
    unsigned char getStreamNumber() {
        return collection.size();
    }
    /**
     * get the status of a given Stream
     */
    StreamStatus getStreamStatus(StreamId id) {
        return collection[id].getStatus();
    }
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
    /**
     * Called by receiveSMEs(), used to update the status of the Stream 
     */
    void updateStream(StreamInfo& stream, StreamManagementElement& sme);
    /**
     * Called by receiveSMEs(), used to update the status of the Server
     */
    void updateServer(StreamInfo& server, StreamManagementElement& sme);
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
    std::map<StreamId, StreamInfo> collection;
    /* UpdatableQueue of Info elements to send to the network */
    UpdatableQueue<StreamId, InfoElement> infoQueue;
    /* Flags that record the changes to the Streams */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;
};

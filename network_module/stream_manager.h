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

namespace mxnet {

/**
 * The class StreamManager contains pointers to all the Stream classes
 * and it is used by classes UplinkPhase, StreamManagementContext and ScheduleComputation
 * to get information about opened streams.
 */

class StreamManager {
public:
    StreamManager() {};
    ~StreamManager() {};

    // Used by the Stream class to register itself in the Stream Map
    void registerStream(StreamInfo info, Stream* stream);
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
    unsigned char getStreamNumber() { return streamList.size(); }
    /**
     * @return the number of available SME to send
     * = number of StreamInfo with status LISTEN_REQ or CONNECT_REQ
     */
    unsigned char getNumSME();
    /**
     * @return the list of SME to send on the network,
     * used by UplinkPhase
     */
    std::vector<StreamManagementElement> getSMEs(unsigned char count);
    /**
     * @return true if there are streams not yet scheduled
     */
    bool hasNewStreams();
    /**
     * change the state of the saved Stream
     */
    //TODO: implement
    void setStreamStatus(unsigned int, StreamStatus) const {};
    /**
     * Return vector containing all the streams
     */
    //TODO: implement
    std::vector<StreamManagementElement> getStreams() { return std::vector<StreamManagementElement>(); };
    /**
     * Return vector of stream elements marked as new
     */
    //TODO: implement
    std::vector<StreamManagementElement> getNewStreams() { return std::vector<StreamManagementElement>(); };
    /**
     * Return vector of stream elements marked as established
     */
    //TODO: implement
    std::vector<StreamManagementElement> getEstablishedStreams() { return std::vector<StreamManagementElement>(); };

    /**
     * Return true if the stream list was modified since last time the flag was cleared 
     */
    bool wasModified() const {
        return modified_flag;
    };

    /**
     * Return true if any stream was removed since last time the flag was cleared 
     */
    bool wasRemoved() const {
        return removed_flag;
    };

    /**
     * Return true if any stream was added since last time the flag was cleared 
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

private:
    /* Map containing pointers to Stream classes in this node */
    std::map<StreamId, Stream*> serverMap;
    /* Map containing pointers to StreamServer classes in this node */
    std::map<StreamId, Stream*> streamMap;
    /* List containing information about Streams related to this node */
    std::list<StreamInfo> streamList;
    /* Flags used by the master node to get whether the streams were changed
       IMPORTANT: this bit must be set to true whenever the data structure is modified */
    bool modified_flag = false;
    bool removed_flag = false;
    bool added_flag = false;

    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex stream_mutex;
#else
    std::mutex stream_mutex;
#endif
};

} /* namespace mxnet */

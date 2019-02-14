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
    void registerStream(StreamManagementElement sme, Stream* stream);
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

private:
    /* Map containing all opened streams, associating StreamId to the corresponding
     * stream class pointer */
    std::map<StreamId, Stream*> serverMap;
    /* Map containing pointers to all StreamServers */
    std::map<StreamId, Stream*> streamMap;
    /* List containing pending SME awaiting to be sent */
    std::list<StreamManagementElement> requestList;

    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex stream_mutex;
#else
    std::mutex stream_mutex;
#endif
};

} /* namespace mxnet */

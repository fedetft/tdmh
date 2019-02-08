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

#include "uplink/stream_management/stream_management_context.h"

namespace mxnet {
/**
 * The class StreamManager contains information about all the streams opened from
 * and to the node, together with the relative buffers
 * It acts as a high-level API for opening streams, and send data through
 * streams.
 */

class StreamManager {
public:
    StreamManager(MACContext& ctx) : mac_ctx(ctx) {}
    virtual ~StreamManager() {};

    /* Put data to send through an opened stream in a buffer */
    void put(const void* data, int size, unsigned int port);
    /* Get data received through an opened stream from a buffer */
    void get(void* data, int size, unsigned int port);
    /* Begin accepting incoming stream requests on a given port */
    void listen(unsigned int port);
    /* Open a new stream to a given node on a specific port
     * by sending a StreamRequestElement to the node */
    void connect(unsigned char dstID, unsigned int dstPort, Period period);

    /* ### Not to be called by the end user ### */
    // Used by the DataPhase to put/get data to/from buffers 
    void putBuffer(unsigned int DstPort, Packet& pkt) {
        assert(recvbuffer.size() > DstPort);
        recvbuffer[DstPort] = new Packet(pkt);
    }
    Packet getBuffer(unsigned int SrcPort) {
        //TODO: handle case in which no data is present in the requested buffer
        assert(sendbuffer.size() >  SrcPort);
        Packet retval = *sendbuffer[SrcPort];
        delete sendbuffer[SrcPort];
        return retval;
    }

private:
    /* Reference to MACContext */
    MACContext& mac_ctx;
    /* Vector containing pointers to a packet buffer for every outbound stream */
    std::vector<Packet*> sendbuffer;
    /* Vector containing pointers to a packet buffer for every inbound stream */
    std::vector<Packet*> recvbuffer;
};

class MasterStreamManager : public StreamManager {
public:
    /* Master node constructor (StreamManagementContext) not necessary as streams
       are managed by the scheduler */
    MasterStreamManager(MACContext& ctx) : StreamManager(ctx) {}
    virtual ~MasterStreamManager() {};
};

class DynamicStreamManager : public StreamManager {
public:
    DynamicStreamManager(MACContext& ctx, StreamManagementContext& str_mgmt) : StreamManager(ctx), stream_mgmt(str_mgmt){}
    virtual ~DynamicStreamManager() {};
private:
    /* Reference to StreamManagementContext for sending/receiving StreamManagementElements */
    StreamManagementContext& stream_mgmt;
};

} /* namespace mxnet */

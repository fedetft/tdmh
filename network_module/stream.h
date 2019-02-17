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

#include "tdmh.h"
#include "packet.h"
#include "uplink/stream_management/stream_management_element.h"

namespace mxnet {

class StreamManager;

/**
 * The class Stream represents a single opened stream.
 * It acts as a high-level API for that stream.
 * The stream is opened when the constructor is called, and closed when
 * the destructor is called.
 */
class Stream {
public:
    Stream(MediumAccessController& tdmh, unsigned char dst,
           unsigned char dstPort, Period period, unsigned char payloadSize,
           Direction direction, Redundancy redundancy);
    ~Stream() {};

    /* Put data to send through this stream */
    void send(const void* data, int size);
    /* Get data received from this stream */
    void recv(void* data, int size);

    /* ### Not to be called by the end user ### */
    // Used by the StreamManager class to put/get data to/from buffers 
    Packet getSendBuffer() {
        return sendBuffer;
    }
    void putRecvBuffer(Packet& pkt) {
        recvBuffer = pkt;
    }

private:
    /* Reference to MediumAccessController */
    MediumAccessController& tdmh;
    /* Reference to StreamManager */
    StreamManager* streamMgr;
    /* Packet buffer for transmitting data */
    Packet sendBuffer;
    /* Packet buffer for receiving data */
    Packet recvBuffer;
    /* Information about this Stream */
    StreamInfo info;
};

/**
 * The class StreamServer is created by a node that want to listen for
 * incoming streams. The listen request is forwarded to the master.
 * When a node connects, it returns a Stream object.
 */
class StreamServer {
public:
    StreamServer(MediumAccessController& tdmh, unsigned char dstPort,
                 Period period, unsigned char payloadSize,
                 Direction direction, Redundancy redundancy);
    ~StreamServer() {};


private:
    /* Reference to MediumAccessController */
    MediumAccessController& tdmh;
    /* Reference to StreamManager */
    StreamManager* streamMgr;
    /* Information about this Stream */
    StreamInfo info;
};

} /* namespace mxnet */

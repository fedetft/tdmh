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
#include <queue>
#ifdef _MIOSIX
#include <miosix.h>
#else
#include <mutex>
#include <condition_variable>
#endif

namespace mxnet {

class StreamManager;

/**
 * The class Stream represents a single opened stream.
 * It acts as a high-level API for that stream.
 * The stream is opened when the constructor is called, and closed when
 * the destructor is called.
 */
class Stream {
    friend class StreamServer;
public:
    Stream(MediumAccessController& tdmh, unsigned char dst,
           unsigned char dstPort, Period period, unsigned char payloadSize,
           Direction direction, Redundancy redundancy);
    /* Used to create an empty (CLOSED) stream to open with accept */
    Stream(MediumAccessController& tdmh);
    ~Stream() {};

    /* Called from StreamManager, to update the status of the Stream
     * and wake up the Stream thread */
    void notifyStream(StreamStatus s);
    /* Put data to send through this stream */
    void send(const void* data, int size);
    /* Get data received from this stream */
    int recv(void* data, int maxSize);

    /* ### Not to be called by the end user ### */
    /* Used by the StreamManager class to get data from buffer */
    Packet getSendBuffer();
    /* Used by the StreamManager class to put data to buffer */
    void putRecvBuffer(Packet& pkt);

private:
    /* Used by the constructor to register the Stream in the StreamManager */
    void registerStream(StreamInfo i);
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
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex stream_mutex;
    miosix::Mutex send_mutex;
    miosix::Mutex recv_mutex;
    miosix::ConditionVariable stream_cv;
    miosix::ConditionVariable send_cv;
    miosix::ConditionVariable recv_cv;
#else
    std::mutex stream_mutex;
    std::mutex send_mutex;
    std::mutex recv_mutex;
    std::condition_variable stream_cv;
    std::condition_variable send_cv;
    std::condition_variable recv_cv;
#endif
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
    /**
     * Called from StreamManager, to update the status of the StreamServer
     * and wake up the StreamServer thread */
    void notifyServer(StreamStatus s);
    /**
     * Called from StreamManager, to notify the StreamServer that a Stream
     * has been opened and wake up the StreamServer thread */
    void openStream(StreamInfo info);
    /**
     * Opens a Stream object by modifying an empty stream.
     * This function blocks unless a stream is opened
     */
    void accept(Stream& stream);

private:
    /* Reference to MediumAccessController */
    MediumAccessController& tdmh;
    /* Reference to StreamManager */
    StreamManager* streamMgr;
    /* Information about this Stream */
    StreamInfo info;
    /* Queue of opened Stream information (used for creating Stream classes) */
    std::queue<StreamInfo> streamQueue;
    /* Thread synchronization */
#ifdef _MIOSIX
    miosix::Mutex server_mutex;
    miosix::ConditionVariable server_cv;
    miosix::ConditionVariable stream_cv;
#else
    std::mutex server_mutex;
    std::condition_variable server_cv;
    std::condition_variable stream_cv;
#endif
};

} /* namespace mxnet */

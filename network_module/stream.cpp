/***************************************************************************
 *   Copyright (C)  2018 by Federico Amedeo Izzo                           *
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

#include "stream.h"
#include "mac_context.h"

namespace mxnet {

Stream::Stream(MediumAccessController& tdmh, unsigned char dst,
               unsigned char dstPort, Period period, unsigned char payloadSize,
               Direction direction, Redundancy redundancy=Redundancy::NONE) : tdmh(tdmh) {
    // Save Stream parameters in StreamInfo
    MACContext* ctx = tdmh.getMACContext();
    streamMgr = ctx->getStreamManager();
    unsigned char src = ctx->getNetworkId();
    /* TODO: Implement srcPort, for the moment it is hardcoded to 0 */
    unsigned char srcPort = 0;
    StreamInfo i = StreamInfo(src, dst, srcPort, dstPort, period, payloadSize,
                      direction, redundancy, StreamStatus::CONNECT_REQ);
    registerStream(i);
}

Stream::Stream(MediumAccessController& tdmh) : tdmh(tdmh) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    MACContext* ctx = tdmh.getMACContext();
    streamMgr = ctx->getStreamManager();
    // Mark stream CLOSED
    info.setStatus(StreamStatus::CLOSED);
}

void Stream::registerStream(StreamInfo i) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
    std::unique_lock<std::mutex> lck(stream_mutex);
#endif
    // Set StreamInfo field
    info = i;
    // Register Stream to StreamManager
    streamMgr->registerStream(info, this);
    // Wait for notification from StreamStatus
    while(info.getStatus() == StreamStatus::CONNECT_REQ) {
        // Condition variable to wait for notification from StreamManager
        stream_cv.wait(lck);
    }
    if(info.getStatus() == StreamStatus::REJECTED)
        throw std::runtime_error("The stream cannot be routed or scheduled");
}

void Stream::notifyStream(StreamStatus s) {
    // Update the stream status
    info.setStatus(s);
    // Wake up the Stream thread
#ifdef _MIOSIX
    stream_cv.signal();
#else
    stream_cv.notify_one();
#endif
}

void Stream::send(const void* data, int size) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(send_mutex);
#else
    std::unique_lock<std::mutex> lck(send_mutex);
#endif
    // Wait for sendBuffer to be empty
    while(sendBuffer.size() != 0) {
        // Condition variable to wait for buffer to be empty
        send_cv.wait(lck);
    }
    sendBuffer.put(data, size);
}

void Stream::recv(void* data, int size) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(recv_mutex);
#else
    std::unique_lock<std::mutex> lck(recv_mutex);
#endif
    // Wait for recvBuffer to be non empty
    while(recvBuffer.size() == 0) {
        // Condition variable to wait for buffer to be non empty
        recv_cv.wait(lck);
    }
}


Packet Stream::getSendBuffer() {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(send_mutex);
#else
    std::unique_lock<std::mutex> lck(send_mutex);
#endif
    return sendBuffer;
    // Clear buffer
    sendBuffer.clear();
    // Wake up the Application thread calling the send
#ifdef _MIOSIX
    send_cv.signal();
#else
    send_cv.notify_one();
#endif
}

void Stream::putRecvBuffer(Packet& pkt) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(recv_mutex);
#else
    std::unique_lock<std::mutex> lck(recv_mutex);
#endif
    recvBuffer = pkt;
    // Wake up the Application thread calling the recv
#ifdef _MIOSIX
    recv_cv.signal();
#else
    recv_cv.notify_one();
#endif

}

StreamServer::StreamServer(MediumAccessController& tdmh, unsigned char dstPort,
                           Period period, unsigned char payloadSize,
                           Direction direction, Redundancy redundancy=Redundancy::NONE) : tdmh(tdmh) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(server_mutex);
#else
    std::unique_lock<std::mutex> lck(server_mutex);
#endif
    // Save Stream parameters in StreamInfo
    MACContext* ctx = tdmh.getMACContext();
    streamMgr = ctx->getStreamManager();
    unsigned char dst = ctx->getNetworkId();
    // NOTE: The StreamId of a LISTEN request is StreamId(dst, dst, 0, dstPort)
    info = StreamInfo(dst, dst, 0, dstPort, period, payloadSize,
                      direction, redundancy);
    // Register Stream to StreamManager
    streamMgr->registerStreamServer(info, this);
    // Wait for notification from StreamStatus
    while(info.getStatus() == StreamStatus::LISTEN_REQ) {
        // Condition variable to wait for notification from StreamManager
        server_cv.wait(lck);
    }
    if(info.getStatus() != StreamStatus::LISTEN &&
       info.getStatus() != StreamStatus::ACCEPTED)
        throw std::runtime_error("Server opening failed!");
}

void StreamServer::notifyServer(StreamStatus s) {
    // Update the stream status
    info.setStatus(s);
    // Wake up the Stream thread
#ifdef _MIOSIX
    server_cv.signal();
#else
    server_cv.notify_one();
#endif
}

void StreamServer::openStream(StreamInfo info) {
    // Push new stream info to queue
    streamQueue.push(info);
    // Wake up the Stream thread
#ifdef _MIOSIX
    stream_cv.signal();
#else
    stream_cv.notify_one();
#endif
}

void StreamServer::accept(Stream& stream) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(server_mutex);
#else
    std::unique_lock<std::mutex> lck(server_mutex);
#endif
    // Wait for opened stream
    while(streamQueue.empty()) {
        // Condition variable to wait for opened streams
        stream_cv.wait(lck);
    }
    printf("Server: Accepted a stream\n");
    StreamInfo info = streamQueue.front();
    streamQueue.pop();
    stream.registerStream(info);
}

}

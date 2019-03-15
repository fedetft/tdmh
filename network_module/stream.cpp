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
#include "debug_settings.h"
#include <algorithm>

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
    while(info.getStatus() != StreamStatus::ESTABLISHED &&
          info.getStatus() != StreamStatus::REJECTED) {
        // Condition variable to wait for notification from StreamManager
        stream_cv.wait(lck);
    }
    if(info.getStatus() == StreamStatus::REJECTED)
        throw std::runtime_error("The stream cannot be routed or scheduled");
}

void Stream::deregisterStream() {
#ifdef _MIOSIX
        miosix::Lock<miosix::Mutex> lck(stream_mutex);
#else
        std::unique_lock<std::mutex> lck(stream_mutex);
#endif
        // Deregister Stream from StreamManager
        streamMgr->deregisterStream(info);
}

void Stream::notifyStream(StreamStatus s) {
    //FIXME: remove this print_dbg
    print_dbg("[S] Calling notifyStream with status %d\n", s);
    // Update the stream status
    info.setStatus(s);
    // Wake up the constructor
#ifdef _MIOSIX
    stream_cv.signal();
#else
    stream_cv.notify_one();
#endif
    // Wake up the send and receive methods
#ifdef _MIOSIX
    send_cv.signal();
    recv_cv.signal();
#else
    send_cv.notify_one();
    recv_cv.notify_one();
#endif
}

void Stream::send(const void* data, int size) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(send_mutex);
#else
    std::unique_lock<std::mutex> lck(send_mutex);
#endif
    // Wait for sendBuffer to be empty or stream CLOSED
    while(sendBuffer.size() != 0 && info.getStatus() != StreamStatus::CLOSED) {
        // Condition variable to wait for buffer to be empty
        send_cv.wait(lck);
    }
    if(info.getStatus() == StreamStatus::CLOSED)
        return;
    else
        sendBuffer.put(data, size);
}

int Stream::recv(void* data, int maxSize) {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(recv_mutex);
#else
    std::unique_lock<std::mutex> lck(recv_mutex);
#endif
    // Wait for recvBuffer to be non empty
    while(recvBuffer.size() == 0 && info.getStatus() != StreamStatus::CLOSED) {
        // Condition variable to wait for buffer to be non empty
        recv_cv.wait(lck);
    }
    if(info.getStatus() == StreamStatus::CLOSED)
        return -1;
    else {
        auto size = std::min<int>(maxSize, recvBuffer.size());
        try {
            recvBuffer.get(data, size);
            return size;
        }
        // Received wrong size packet
        catch(PacketUnderflowException& ){
            return -1;
        }
    }
}

Packet Stream::getSendBuffer() {
    Redundancy r = info.getRedundancy();
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(send_mutex);
#else
    std::unique_lock<std::mutex> lck(send_mutex);
#endif
    auto result = sendBuffer;
    // No redundancy: send value once
    if(r == Redundancy::NONE) {
        // Clear buffer
        sendBuffer.clear();
        // Wake up the Application thread calling the send
#ifdef _MIOSIX
        send_cv.signal();
#else
        send_cv.notify_one();
#endif
    }
    // Double redundancy: send value twice before clear and notify
    else if((r == Redundancy::DOUBLE) ||
       (r == Redundancy::DOUBLE_SPATIAL)) {
        if(++timesSent == 2){
            timesSent = 0;
            sendBuffer.clear();
#ifdef _MIOSIX
            send_cv.signal();
#else
            send_cv.notify_one();
#endif
        }
    }
    // Triple redundancy: send value three times before clear and notify
    else if((r == Redundancy::TRIPLE) ||
       (r == Redundancy::TRIPLE_SPATIAL)) {
        if(++timesSent == 3){ 
            timesSent = 0;
            sendBuffer.clear();
#ifdef _MIOSIX
            send_cv.signal();
#else
            send_cv.notify_one();
#endif
        }  
    }
    return result;
}

void Stream::putRecvBuffer(Packet& pkt) {
    Redundancy r = info.getRedundancy();
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(recv_mutex);
#else
    std::unique_lock<std::mutex> lck(recv_mutex);
#endif
    // Avoid overwriting valid data
    if(pkt.size() != 0)
        recvBuffer = pkt;
    // No redundancy: notify right away
    if(r == Redundancy::NONE) {
        // Wake up the Application thread calling the recv
#ifdef _MIOSIX
        recv_cv.signal();
#else
        recv_cv.notify_one();
#endif
    }
    // Double redundancy: notify after receiving twice
    else if((r == Redundancy::DOUBLE) ||
       (r == Redundancy::DOUBLE_SPATIAL)) {
        if(++timesRecv == 2){
            timesRecv = 0;
#ifdef _MIOSIX
            recv_cv.signal();
#else
            recv_cv.notify_one();
#endif
        }
    }
    // Triple redundancy: notify after receiving three times
    else if((r == Redundancy::TRIPLE) ||
       (r == Redundancy::TRIPLE_SPATIAL)) {
        if(++timesRecv == 3){ 
            timesRecv = 0;
#ifdef _MIOSIX
            recv_cv.signal();
#else
            recv_cv.notify_one();
#endif
        }  
    }
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

void StreamServer::deregisterStreamServer() {
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(server_mutex);
#else
    std::unique_lock<std::mutex> lck(server_mutex);
#endif
    // Deregister StreamServer from StreamManager
    streamMgr->deregisterStreamServer(info);
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
}

void StreamServer::wakeAccept() {
    // Wake up the Stream thread
#ifdef _MIOSIX
    stream_cv.signal();
#else
    stream_cv.notify_one();
#endif 
}

void StreamServer::accept(std::list<std::shared_ptr<Stream>>& stream) {
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
    // The StreamServer was closed
    if(info.getStatus() == StreamStatus::CLOSED)
        return;
    while(!streamQueue.empty()) {
        StreamInfo info = streamQueue.front();
        StreamId id = info.getStreamId();
        print_dbg("[S] StreamServer: Accepted the stream (%d,%d)\n", id.src, id.dst);
        streamQueue.pop();
        auto* newStream = new Stream(tdmh);
        newStream->registerStream(info);
        std::shared_ptr<Stream> ptr(newStream);
        stream.push_back(ptr);
    }
}

}

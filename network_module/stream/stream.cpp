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
#include "../mac_context.h"
#include "../util/debug_settings.h"
#include <algorithm>

namespace mxnet {

int Stream::connect(StreamManager* mgr) {
    // Send CONNECT SME
    mgr->enqueueSME(StreamManagementElement(info, SMEType::CONNECT));
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        // Wait to get out of CONNECTING status
        for(;;) {
            if(getStatus() != StreamStatus::CONNECTING) break;
            // Condition variable to wait for addedStream().
            connect_cv.wait(lck);
        }
        auto status = getStatus();
        if(status == StreamStatus::CONNECT_FAILED)
            return -1;
        if(status == StreamStatus::ESTABLISHED)
            return 0;
        //TODO: check if we can end up in other states
        else
            return -2;
    }
}

int Stream::write(const void* data, int size) {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(send_mutex);
#else
    std::unique_lock<std::mutex> lck(send_mutex);
#endif
    // Wait for sendBuffer to be empty or stream to be not ESTABLISHED
    while(sendBuffer.size() != 0 && getStatus() == StreamStatus::ESTABLISHED) {
        // Condition variable to wait for buffer to be empty
        send_cv.wait(lck);
    }
    if(info.getStatus() != StreamStatus::ESTABLISHED)
        return -1;
    else
        sendBuffer.put(data, size);
    //TODO: Packet::put() do not return number of copied bytes
    return 0;
}

int Stream::read(void* data, int maxSize) {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(recv_mutex);
#else
    std::unique_lock<std::mutex> lck(recv_mutex);
#endif
    // Wait for recvBuffer to be non empty
    while(recvBuffer.size() == 0 && info.getStatus() == StreamStatus::ESTABLISHED) {
        // Condition variable to wait for buffer to be non empty
        recv_cv.wait(lck);
    }
    if(info.getStatus() != StreamStatus::ESTABLISHED)
        return -1;
    else {
        auto size = std::min<int>(maxSize, recvBuffer.size());
        try {
            recvBuffer.get(data, size);
            recvBuffer.clear();
            return size;
        }
        // Received wrong size packet
        catch(PacketUnderflowException& ){
            return -1;
        }
    }
}

void Stream::putPacket(const Packet& data) {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    Redundancy r = info.getRedundancy();
    status_mutex.unlock();

    // Lock mutex for concurrent access at recvBuffer
    recv_mutex.lock();
    // Avoid overwriting valid data
    if(data.size() != 0)
        recvBuffer = data;

    bool wakeRead = false;
    //TODO: Maybe it's better to handle redundancy
    // with calls from the dataphase
    // No redundancy: notify right away
    if(r == Redundancy::NONE) {
        wakeRead = true;
    }
    // Double redundancy: notify after receiving twice
    else if((r == Redundancy::DOUBLE) ||
            (r == Redundancy::DOUBLE_SPATIAL)) {
        if(++timesRecv >= 2){
            timesRecv = 0;
            wakeRead = true;
        }
    }
    // Triple redundancy: notify after receiving three times
    else if((r == Redundancy::TRIPLE) ||
            (r == Redundancy::TRIPLE_SPATIAL)) {
        if(++timesRecv >= 3){ 
            timesRecv = 0;
            wakeRead = true;
        }  
    }
    recv_mutex.unlock();
    if(wakeRead) {
        // Wake up the read method
#ifdef _MIOSIX
        recv_cv.signal();
#else
        recv_cv.notify_one();
#endif
    }
}

void Stream::getPacket(Packet& data) {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    Redundancy r = info.getRedundancy();
    status_mutex.unlock();

    // Lock mutex for concurrent access at recvBuffer
    send_mutex.lock();
    data = sendBuffer;

    bool wakeWrite = false;
    //TODO: Maybe it's better to handle redundancy
    // with calls from the dataphase
    // No redundancy: send value once
    if(r == Redundancy::NONE) {
        sendBuffer.clear();
        wakeWrite = true;
    }
    // Double redundancy: send value twice before clear and notify
    else if((r == Redundancy::DOUBLE) ||
            (r == Redundancy::DOUBLE_SPATIAL)) {
        if(++timesSent >= 2){
            timesSent = 0;
            sendBuffer.clear();
            wakeWrite = true;
        }
    }
    // Triple redundancy: send value three times before clear and notify
    else if((r == Redundancy::TRIPLE) ||
            (r == Redundancy::TRIPLE_SPATIAL)) {
        if(++timesSent >= 3){ 
            timesSent = 0;
            sendBuffer.clear();
            wakeWrite = true;
        }  
    }
    send_mutex.unlock();
    if(wakeWrite) {
        // Wake up the write method
#ifdef _MIOSIX
        send_cv.signal();
#else
        send_cv.notify_one();
#endif
    }
}

void Stream::addedStream() {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::CONNECTING:
        setStatus(StreamStatus::ESTABLISHED);
        break;
    case StreamStatus::REMOTELY_CLOSED:
        setStatus(StreamStatus::REOPENED);
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
}

bool Stream::removedStream() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::ACCEPT_WAIT:
            deletable = true;
            break;
    case StreamStatus::ESTABLISHED:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::REOPENED:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::CLOSE_WAIT:
        deletable = true;
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the write and read methods
#ifdef _MIOSIX
    send_cv.signal();
    recv_cv.signal();
#else
    send_cv.notify_one();
    recv_cv.notify_one();
#endif
    return deletable;
}

void Stream::rejectedStream() {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::CONNECTING:
        setStatus(StreamStatus::CONNECT_FAILED);
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
}

void Stream::closedServer(StreamManager* mgr) {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::ACCEPT_WAIT:
        setStatus(StreamStatus::CLOSE_WAIT);
        // Send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    default:
        break;
    }
    status_mutex.unlock();
}

bool Stream::close(StreamManager* mgr) {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::ESTABLISHED:
        setStatus(StreamStatus::CLOSE_WAIT);
        // Send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    case StreamStatus::REMOTELY_CLOSED:
        deletable = true;
        break;
    case StreamStatus::REOPENED:
        setStatus(StreamStatus::CLOSE_WAIT);
        break;
    case StreamStatus::CLOSE_WAIT:
        //NOTE: if we were created in CLOSE_WAIT, we need to send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the write and read methods
#ifdef _MIOSIX
    send_cv.signal();
    recv_cv.signal();
#else
    send_cv.notify_one();
    recv_cv.notify_one();
#endif
    return deletable;
}

void Stream::periodicUpdate(StreamManager* mgr) {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::CONNECTING:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CONNECT SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CONNECT));
        }
        break;
    case StreamStatus::REOPENED:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        }
        break;
    case StreamStatus::CLOSE_WAIT:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        }
        break;
    default:
        break;
    }
    status_mutex.unlock();
}

bool Stream::desync() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::CONNECTING:
        setStatus(StreamStatus::CONNECT_FAILED);
        break;
    case StreamStatus::ACCEPT_WAIT:
        deletable = true;
        break;
    case StreamStatus::ESTABLISHED:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::REOPENED:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::CLOSE_WAIT:
        deletable = true;
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
    // Wake up the write and read methods
#ifdef _MIOSIX
    send_cv.signal();
    recv_cv.signal();
#else
    send_cv.notify_one();
    recv_cv.notify_one();
#endif
    return deletable;
}

int Server::listen(StreamManager* mgr) {
    // Send LISTEN SME
    mgr->enqueueSME(StreamManagementElement(info, SMEType::LISTEN));
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        // Wait to get out of LISTEN_WAIT status
        for(;;) {
            if(getStatus() != StreamStatus::LISTEN_WAIT) break;
            // Condition variable to wait for acceptedServer().
            listen_cv.wait(lck);
        }
        auto status = getStatus();
        if(status == StreamStatus::LISTEN_FAILED)
            return -1;
        if(status == StreamStatus::LISTEN)
            return 0;
        else
            return -2;
    }
}

int Server::accept() {
    // Lock mutex for concurrent access at StreamInfo
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
    std::unique_lock<std::mutex> lck(status_mutex);
#endif
    // Return error if server not open
    if(getStatus() != StreamStatus::LISTEN)
        return -1;

    // Wait for opened stream
    while(pendingAccept.empty()) {
        // Condition variable to wait for opened streams
        listen_cv.wait(lck);
    }
    // Return error if server not open
    if(getStatus() != StreamStatus::LISTEN)
        return -1;
    auto it = pendingAccept.begin();
    int fd = *it;
    pendingAccept.erase(it);
    return fd;
}

void Server::addPendingStream(int fd) {
    // Lock mutex for concurrent access at pendingAccept
    status_mutex.lock();
    // Push add new stream fd to set
    pendingAccept.insert(fd);
    status_mutex.unlock();
}

void Server::acceptedServer() {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::LISTEN_WAIT:
        setStatus(StreamStatus::LISTEN);
        break;
    case StreamStatus::REMOTELY_CLOSED:
        setStatus(StreamStatus::REOPENED);
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the listen() method
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
}

void Server::rejectedServer() {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::LISTEN_WAIT:
        setStatus(StreamStatus::LISTEN_FAILED);
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the listen() method
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
}

bool Server::close(StreamManager* mgr) {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::LISTEN:
        setStatus(StreamStatus::CLOSE_WAIT);
        // Send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    case StreamStatus::REMOTELY_CLOSED:
        deletable = true;
        break;
    case StreamStatus::REOPENED:
        setStatus(StreamStatus::CLOSE_WAIT);
        break;
    case StreamStatus::CLOSE_WAIT:
        //NOTE: if we were created in CLOSE_WAIT, we need to send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    default:
        break;
    }
    // Close pendingAccept Streams
    for(auto fd : pendingAccept) {
        mgr->closedServer(fd);
    }
    status_mutex.unlock();
    // Wake up the listen and accept methods
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
    return deletable;
}

void Server::periodicUpdate(StreamManager* mgr) {
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::LISTEN_WAIT:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send LISTEN SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::LISTEN));
        }
        if(failTimeout-- <= 0) {
            // Give up opening server and close it
            setStatus(StreamStatus::LISTEN_FAILED);
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        }
        break;
    case StreamStatus::REOPENED:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        }
        break;
    case StreamStatus::CLOSE_WAIT:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        }
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the listen and accept methods
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
}

bool Server::desync() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    status_mutex.lock();
    switch(getStatus()) {
    case StreamStatus::LISTEN_WAIT:
        setStatus(StreamStatus::LISTEN_FAILED);
        break;
    case StreamStatus::LISTEN:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::REOPENED:
        setStatus(StreamStatus::REMOTELY_CLOSED);
        break;
    case StreamStatus::CLOSE_WAIT:
        deletable = true;
        break;
    default:
        break;
    }
    status_mutex.unlock();
    // Wake up the listen and accept methods
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
    return deletable;
}

} /* namespace mxnet */

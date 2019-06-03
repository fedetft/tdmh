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
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        // Send CONNECT SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CONNECT));
        // Wait to get out of CONNECTING status
        for(;;) {
            if(info.getStatus() != StreamStatus::CONNECTING) break;
            // Condition variable to wait for addedStream().
            connect_cv.wait(lck);
        }
        auto status = info.getStatus();
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
    miosix::Lock<miosix::FastMutex> lck(tx_mutex);
#else
    std::unique_lock<std::mutex> lck(tx_mutex);
#endif
    if(nextTxPacketReady == false) {
        try {
            nextTxPacket.put(data, size);
            nextTxPacketReady = true;
            return size;
        }
        // Calling write with size too big
        catch(PacketOverflowException& ){
            return -1;
        }
    }
    // Wait for txWakeUp condition to be true
    while(txWakeUp == false){
        tx_cv.wait(lck);
    }
    // Reset txWakeup variable
    txWakeUp = false;
    if(nextTxPacketReady == false) {
        try {
            nextTxPacket.put(data, size);
            nextTxPacketReady = true;
            return size;
        }
        // Calling write with size too big
        catch(PacketOverflowException& ){
            return -1;
        }
    }
    // The stream was closed
    if(info.getStatus() != StreamStatus::ESTABLISHED) { 
        return -2;
    }
    // We did not send any data this round
    else {
        return -1;
    }
}

int Stream::read(void* data, int maxSize) {
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(rx_mutex);
#else
    std::unique_lock<std::mutex> lck(rx_mutex);
#endif
    // Wait for next Period
    while(alreadyReceivedShared == true && info.getStatus() == StreamStatus::ESTABLISHED) { 
        rx_cv.wait(lck);
    }
    // The stream was closed
    if(info.getStatus() != StreamStatus::ESTABLISHED) { 
        return -2;
    }
    if(receivedShared == true) {
        receivedShared = false;
        alreadyReceivedShared = true;
        auto size = std::min<int>(maxSize, rxPacketShared.size());
        try {
            rxPacketShared.get(data, size);
            rxPacketShared.clear();
            return size;
        }
        // Received wrong size packet
        catch(PacketUnderflowException& ){
            return -1;
        }
    }
    // We did not receive any data this round
    return -1;
}

void Stream::receivePacket(const Packet& data) {
    // NOTE: we use a duplicated rxPacket to acquire data before locking the mutex
    rxPacket = data;
    received = true;
    updateRxPacket();
}

void Stream::missPacket() {
    // No data to receive
    updateRxPacket();
}

bool Stream::sendPacket(Packet& data) {
    // Stream Redundancy logic
    if(++txCount >= redundancyCount) {
        // Reset received packet counter
        txCount = 0;
        {
            // Lock mutex for shared access with application thread
#ifdef _MIOSIX
            miosix::Lock<miosix::FastMutex> lck(tx_mutex);
#else
            std::unique_lock<std::mutex> lck(tx_mutex);
#endif
            if(nextTxPacketReady == true) {
                txPacket = nextTxPacket;
                txPacketReady = true;
                nextTxPacket.clear();
                nextTxPacketReady = false;
            }
            // Wake up the write method
            txWakeUp = true;
#ifdef _MIOSIX
            tx_cv.signal();
#else
            tx_cv.notify_one();
#endif
        }
    }
    if(txPacketReady) {
        data = txPacket;
        return true;
    }
    else
        return false;
}

void Stream::addedStream(StreamParameters newParams) {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::CONNECTING:
            setStatus(StreamStatus::ESTABLISHED);
            break;
        case StreamStatus::REMOTELY_CLOSED:
            setStatus(StreamStatus::REOPENED);
            break;
        default:
            break;
        }
        // NOTE: Update stream parameters and cached redundancy,
        // they may have changed after negotiation with server
        info.setParams(newParams);
        updateRedundancy();
}
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
}

void Stream::acceptedStream() {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::ACCEPT_WAIT:
            setStatus(StreamStatus::ESTABLISHED);
            break;
        default:
            break;
        }
    }
}

bool Stream::removedStream() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
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
    }
    // Wake up the write and read methods
    wakeWriteRead();
    return deletable;
}

void Stream::rejectedStream() {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::CONNECTING:
            setStatus(StreamStatus::CONNECT_FAILED);
            break;
        default:
            break;
        }
    }
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
}

void Stream::closedServer(StreamManager* mgr) {
    // Lock mutex for concurrent access at StreamInfo
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
    std::unique_lock<std::mutex> lck(status_mutex);
#endif
    switch(info.getStatus()) {
    case StreamStatus::ACCEPT_WAIT:
        setStatus(StreamStatus::CLOSE_WAIT);
        // Send CLOSED SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
        break;
    default:
        break;
    }
}

bool Stream::close(StreamManager* mgr) {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
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
    }
    // Wake up the write and read methods
    wakeWriteRead();
    return deletable;
}

void Stream::periodicUpdate(StreamManager* mgr) {
    bool changed = false;
    // Lock mutex for concurrent access at StreamInfo
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
    std::unique_lock<std::mutex> lck(status_mutex);
#endif
    switch(info.getStatus()) {
    case StreamStatus::CONNECTING:
        if(smeTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            // Send CONNECT SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CONNECT));
        }
        if(failTimeout-- <= 0) {
            smeTimeout = smeTimeoutMax;
            failTimeout = failTimeoutMax;
            // Give up opening stream and close it
            setStatus(StreamStatus::CONNECT_FAILED);
            // Send CLOSED SME
            mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
            changed = true;
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
    if(changed) {
        // Wake up the connect method
#ifdef _MIOSIX
        connect_cv.signal();
#else
        connect_cv.notify_one();
#endif
    }
}

bool Stream::desync() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
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
    }
    // Wake up the connect() method
#ifdef _MIOSIX
    connect_cv.signal();
#else
    connect_cv.notify_one();
#endif
    // Wake up the write and read methods
    wakeWriteRead();
    return deletable;
}


void Stream::updateRedundancy() {
    redundancy = info.getRedundancy();
    // No redundancy: notify after receiving
    if(redundancy == Redundancy::NONE) {
        redundancyCount = 1;
    }
    // Double redundancy: notify after receiving twice
    else if((redundancy == Redundancy::DOUBLE) ||
            (redundancy == Redundancy::DOUBLE_SPATIAL)) {
        redundancyCount = 2;
    }
    // Triple redundancy: notify after receiving three times
    else if((redundancy == Redundancy::TRIPLE) ||
            (redundancy == Redundancy::TRIPLE_SPATIAL)) {
        redundancyCount = 3;
    }
}

void Stream::wakeWriteRead() {
    {    // Lock mutex for shared access with application thread
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(tx_mutex);
#else
        std::unique_lock<std::mutex> lck(tx_mutex);
#endif
        txWakeUp = true;
        // Wake up the write method
#ifdef _MIOSIX
        tx_cv.signal();
#else
        tx_cv.notify_one();
#endif
        }
    {
        // Lock mutex for shared access with application thread
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(rx_mutex);
#else
        std::unique_lock<std::mutex> lck(rx_mutex);
#endif
        // Wake up the read method
#ifdef _MIOSIX
        rx_cv.signal();
#else
        rx_cv.notify_one();
#endif
    }
}

void Stream::updateRxPacket() {
    // Stream Redundancy logic
    if(++rxCount >= redundancyCount) {
        // Reset received packet counter
        rxCount = 0;
        {
            // Lock mutex for shared access with application thread
#ifdef _MIOSIX
            miosix::Lock<miosix::FastMutex> lck(rx_mutex);
#else
            std::unique_lock<std::mutex> lck(rx_mutex);
#endif
            // Copy received packet to variabled shared with application
            rxPacketShared = rxPacket;
            receivedShared = received;
            alreadyReceivedShared = false;
            rxPacket.clear();
            received = false;
            // Wake up the read method
#ifdef _MIOSIX
            rx_cv.signal();
#else
            rx_cv.notify_one();
#endif
        }
    }
}

int Server::listen(StreamManager* mgr) {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        // Send LISTEN SME
        mgr->enqueueSME(StreamManagementElement(info, SMEType::LISTEN));
        // Wait to get out of LISTEN_WAIT status
        for(;;) {
            if(info.getStatus() != StreamStatus::LISTEN_WAIT) break;
            // Condition variable to wait for acceptedServer().
            listen_cv.wait(lck);
        }
        auto status = info.getStatus();
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
    if(info.getStatus() != StreamStatus::LISTEN)
        return -1;

    // Wait for opened stream
    while(pendingAccept.empty()) {
        // Condition variable to wait for opened streams
        listen_cv.wait(lck);
    }
    // Return error if server not open
    if(info.getStatus() != StreamStatus::LISTEN)
        return -1;
    // Return first stream fd from pendingAccept
    auto it = pendingAccept.begin();
    int fd = *it;
    pendingAccept.erase(it);
    return fd;
}

void Server::addPendingStream(int fd) {
    // Lock mutex for concurrent access at pendingAccept
#ifdef _MIOSIX
    miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
    std::unique_lock<std::mutex> lck(status_mutex);
#endif
    // Push add new stream fd to set
    pendingAccept.insert(fd);
    // Wake up the accept() method
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
}

void Server::acceptedServer() {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::LISTEN_WAIT:
            setStatus(StreamStatus::LISTEN);
            break;
        case StreamStatus::REMOTELY_CLOSED:
            setStatus(StreamStatus::REOPENED);
            break;
        default:
            break;
        }
    }
    // Wake up the listen() method
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
}

void Server::rejectedServer() {
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::LISTEN_WAIT:
            setStatus(StreamStatus::LISTEN_FAILED);
            break;
        default:
            break;
        }
    }
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
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
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
    }
    // Wake up the listen and accept methods
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
    return deletable;
}

void Server::periodicUpdate(StreamManager* mgr) {
    bool changed = false;
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
        case StreamStatus::LISTEN_WAIT:
            if(smeTimeout-- <= 0) {
                smeTimeout = smeTimeoutMax;
                // Send LISTEN SME
                mgr->enqueueSME(StreamManagementElement(info, SMEType::LISTEN));
            }
            if(failTimeout-- <= 0) {
                smeTimeout = smeTimeoutMax;
                failTimeout = failTimeoutMax;
                // Give up opening server and close it
                setStatus(StreamStatus::LISTEN_FAILED);
                // Send CLOSED SME
                mgr->enqueueSME(StreamManagementElement(info, SMEType::CLOSED));
                changed = true;
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
    }
    if(changed) {
        // Wake up the listen and accept methods
#ifdef _MIOSIX
        listen_cv.signal();
#else
        listen_cv.notify_one();
#endif
    }
}

bool Server::desync() {
    bool deletable = false;
    // Lock mutex for concurrent access at StreamInfo
    {
#ifdef _MIOSIX
        miosix::Lock<miosix::FastMutex> lck(status_mutex);
#else
        std::unique_lock<std::mutex> lck(status_mutex);
#endif
        switch(info.getStatus()) {
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
    }
    // Wake up the listen and accept methods
#ifdef _MIOSIX
    listen_cv.signal();
#else
    listen_cv.notify_one();
#endif
    return deletable;
}

} /* namespace mxnet */

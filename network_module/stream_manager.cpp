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

#include "stream_manager.h"
#include <algorithm>

namespace mxnet {

void StreamCollection::setStreamStatus(StreamId id, StreamStatus status) {
    collection[id].setStatus(status);
}

std::vector<StreamInfo> StreamCollection::getStreams() {
    std::vector<StreamInfo> result;
    for(auto& stream : collection)
        result.push_back(stream.second);
    return result;
}

bool isSchedulable(std::pair<StreamId,StreamInfo> stream) {
    StreamStatus status = stream.second.getStatus();
    return (status == StreamStatus::ACCEPTED);
}

bool StreamCollection::hasSchedulableStreams() {
    return std::count_if(collection.begin(), collection.end(), isSchedulable);
}

std::vector<StreamInfo> StreamCollection::getStreamsWithStatus(StreamStatus s) {
    std::vector<StreamInfo> result;
    for (auto& stream: collection) {
        if(stream.second.getStatus() == s)
            result.push_back(stream.second);
    }
    return result;
}

void StreamManager::registerStream(StreamInfo info, Stream* client) {
    printf("[SM] Stream registered! \n");
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    StreamId id = info.getStreamId();
    // Register Stream class pointer in client map
    clientMap[info.getStreamId()] = client;
    // Register Stream information and status
    streamMap[info.getStreamId()] = StreamInfo(info, StreamStatus::CONNECT_REQ);
    // Push corresponding SME on the queue
    smeQueue.push(StreamManagementElement(info, StreamStatus::CONNECT));
    // Set flags
    modified_flag = true;
    added_flag = true;
}

void StreamManager::deregisterStream(StreamInfo info) {
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    // Remove Stream class pointer in stream map
    clientMap.erase(info.getStreamId());
    // Mark Stream as closed
    streamMap[info.getStreamId()].setStatus(StreamStatus::CLOSED);
    // Send SME only if we are in a dynamic node
    if(myId != 0) {
        // Push corresponding SME on the queue
        smeQueue.push(StreamManagementElement(info, StreamStatus::CLOSED));
    }
    // Set flags
    modified_flag = true;
    removed_flag = true;
}

void StreamManager::registerStreamServer(StreamInfo info, StreamServer* server) {
    printf("[SM] StreamServer registered! \n");
    // Mutex lock to access the Stream map from the application thread.
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    // Register StreamServer class pointer in server map
    serverMap[info.getStreamId()] = server;
    // If we are in a dynamic node, send SME
    if(myId != 0) {
        // Register Stream information and status
        streamMap[info.getStreamId()] = StreamInfo(info, StreamStatus::LISTEN_REQ);
        // Push corresponding SME on the queue
        smeQueue.push(StreamManagementElement(info, StreamStatus::LISTEN));
    }
    // If we are in the master node, no need to send SME
    else {
        // Register Stream information and status
        streamMap[info.getStreamId()] = StreamInfo(info, StreamStatus::LISTEN);
        server->notifyServer(StreamStatus::LISTEN);
    }
    // Set flags
    modified_flag = true;
    added_flag = true;
}

void StreamManager::notifyStreams(const std::vector<ExplicitScheduleElement>& schedule) {
    for(auto& elem : schedule) {
        StreamId id = elem.getStreamId();
        /* If schedule element action is SLEEP, ignore */
        if(elem.getAction() != Action::SLEEP) {
            StreamInfo info = elem.getStreamInfo();
            StreamId listenId(id.dst, id.dst, 0, id.dstPort);
            // If Stream is registered on this node, set status to ESTABLISHED
            if (clientMap.find(id) != clientMap.end()) {
                streamMap[id].setStatus(StreamStatus::ESTABLISHED);
                clientMap[id]->notifyStream(StreamStatus::ESTABLISHED);
            }
            // If StreamServer (LISTEN) is registered on this node,
            // and server-side stream is not already open, open it
            if (serverMap.find(listenId) != serverMap.end()) {
                if(clientMap.find(id) == clientMap.end()) {
                    serverMap[listenId]->openStream(info);
                    printf("[SM] node %d, Server Stream %d,%d opened!\n",myId, id.src, id.dst);
                }
                streamMap[id].setStatus(StreamStatus::ESTABLISHED);
            }
        }
    }
}

unsigned char StreamManager::getStreamNumber() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    return streamMap.size();
}

StreamStatus StreamManager::getStreamStatus(StreamId id) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    // Check if stream exists
    if (streamMap.find(id) == streamMap.end())
        return StreamStatus::CLOSED;
    else
        return streamMap[id].getStatus();
}

void StreamManager::setStreamStatus(StreamId id, StreamStatus status) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    // Check if stream exists
    if (streamMap.find(id) != streamMap.end()) {
        printf("[SM] Stream found, changing status\n");
        streamMap[id].setStatus(status);
        // Set flags
        modified_flag = true;
        if(status == StreamStatus::ACCEPTED)
            added_flag = true;
        if(status == StreamStatus::CLOSED)
            removed_flag = true;
        if(status == StreamStatus::REJECTED)
            clientMap[id]->notifyStream(StreamStatus::REJECTED);
    }
    else {
        printf("[SM] Stream not found\n");
    }
}

void StreamManager::addStream(const StreamInfo& stream) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    streamMap[stream.getStreamId()] = stream;
    // Set flags
    modified_flag = true;
    added_flag = true;
}

StreamCollection StreamManager::getSnapshot() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    return StreamCollection(streamMap);
}

unsigned char StreamManager::getNumSME() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    return smeQueue.size();
}

std::vector<StreamManagementElement> StreamManager::dequeueSMEs(unsigned char count) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    unsigned char available = smeQueue.size();
    unsigned char num = std::min(count, available);
    std::vector<StreamManagementElement> result;
    result.reserve(num);
    for(unsigned int i = 0; i < num; i++) {
        result.push_back(smeQueue.front());
        smeQueue.pop();
    }
    return result;
}

void StreamManager::enqueueSMEs(std::vector<StreamManagementElement> smes) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    for(auto& sme: smes) {
        smeQueue.push(sme);
    }
}

unsigned char StreamManager::getNumInfo() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    return infoQueue.size();
}

std::vector<InfoElement> StreamManager::dequeueInfo(unsigned char count) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    unsigned char available = infoQueue.size();
    unsigned char num = std::min(count, available);
    std::vector<InfoElement> result;
    result.reserve(num);
    for(unsigned int i = 0; i < num; i++) {
        result.push_back(infoQueue.front());
        infoQueue.pop();
    }
    return result;
}

void StreamManager::enqueueInfo(std::vector<InfoElement> infos) {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    for(auto& info : infos) {
        infoQueue.push(info);
    }
}

void StreamManager::receiveInfo() {
    // Mutex lock to access the shared container StreamMap
#ifdef _MIOSIX
    miosix::Lock<miosix::Mutex> lck(streamMgr_mutex);
#else
    std::unique_lock<std::mutex> lck(streamMgr_mutex);
#endif
    while(!infoQueue.empty()) {
        InfoElement info = infoQueue.front();
        StreamId id = info.getStreamId();
        //Check if stream exists
        if (streamMap.find(id) != streamMap.end()) {
            switch(info.getType()){
            case InfoType::ACK_LISTEN:
                if(streamMap[id].getStatus() == StreamStatus::LISTEN_REQ) {
                    streamMap[id].setStatus(StreamStatus::LISTEN);
                    // Notify Server thread
                    printf("[SM] StreamServer %d->%d LISTEN\n", id.src, id.dst);
                    serverMap[id]->notifyServer(StreamStatus::LISTEN);
                }
                break;
            case InfoType::NACK_CONNECT:
                if(streamMap[id].getStatus() == StreamStatus::CONNECT_REQ) {
                    streamMap[id].setStatus(StreamStatus::REJECTED);
                    // Notify Stream thread
                    printf("[SM] Stream %d->%d REJECTED\n", id.src, id.dst);
                    clientMap[id]->notifyStream(StreamStatus::REJECTED);
                }
                break;
            }
        }
        infoQueue.pop();
    }
}

} /* namespace mxnet */
